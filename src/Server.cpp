/* ************************************************************************** */
/*                                                                            */
/*   Server.cpp                                                               */
/*                                                                            */
/*   PARTE 1 - Red. Un unico poll() para todos los fds. Reglas del subject:    */
/*     - nunca recv/send sin haber pasado por poll(),                          */
/*     - no se consulta errno para decidir que hacer tras recv/send,           */
/*     - fds no bloqueantes con fcntl(fd, F_SETFL, O_NONBLOCK),                 */
/*     - los paquetes se agregan hasta reconstruir cada linea.                 */
/*                                                                            */
/* ************************************************************************** */

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Server.hpp"
#include "Client.hpp"
#include "ICommandHandler.hpp"

volatile bool	Server::_running = false;

Server::Server(unsigned short port, const std::string &password,
		const std::string &name) :
	_port(port),
	_password(password),
	_name(name),
	_listenFd(-1),
	_handler(0)
{
}

Server::~Server(void)
{
	std::map<int, Client *>::iterator	it;

	for (it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->first >= 0)
			close(it->first);
		delete it->second;
	}
	_clients.clear();
	if (_listenFd >= 0)
		close(_listenFd);
}

void	Server::setCommandHandler(ICommandHandler &handler)
{
	_handler = &handler;
}

void	Server::requestStop(void)
{
	_running = false;
}

/* -------------------------------- arranque -------------------------------- */

void	Server::setNonBlocking(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error("fcntl(F_SETFL, O_NONBLOCK) failed");
}

void	Server::setupListenSocket(void)
{
	struct sockaddr_in	addr;
	int					opt = 1;

	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd < 0)
		throw std::runtime_error("socket() failed");
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
	setNonBlocking(_listenFd);

	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(_port);

	if (bind(_listenFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
		throw std::runtime_error("bind() failed (port in use?)");
	if (listen(_listenFd, SOMAXCONN) < 0)
		throw std::runtime_error("listen() failed");

	addPollFd(_listenFd, POLLIN);
}

/* ------------------------------ pollfd helpers ---------------------------- */

void	Server::addPollFd(int fd, short events)
{
	struct pollfd	pfd;

	pfd.fd = fd;
	pfd.events = events;
	pfd.revents = 0;
	_pollFds.push_back(pfd);
}

void	Server::updatePollEvents(std::size_t index)
{
	Client	*client = _clients[_pollFds[index].fd];

	_pollFds[index].events = POLLIN;
	if (client && client->hasPendingOutput())
		_pollFds[index].events |= POLLOUT;
}

/* ---------------------------------- bucle --------------------------------- */

void	Server::run(void)
{
	if (_handler == 0)
		throw std::runtime_error("Server::run called without a command handler");

	setupListenSocket();
	_running = true;
	std::cout << "ircserv listening on port " << _port << std::endl;

	while (_running)
	{
		int	ready = poll(&_pollFds[0], _pollFds.size(), -1);

		if (ready < 0)
		{
			if (errno == EINTR)
				continue ;
			break ;
		}

		/* Recorremos de atras hacia delante: disconnectClient puede borrar
		 * la entrada actual y reordenar el vector. */
		for (std::size_t i = _pollFds.size(); i-- > 0; )
		{
			short	revents = _pollFds[i].revents;

			if (revents == 0)
				continue ;
			if (_pollFds[i].fd == _listenFd)
			{
				if (revents & POLLIN)
					acceptNewClient();
				continue ;
			}
			if (revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				disconnectClient(i, "Connection error");
				continue ;
			}
			if (revents & POLLOUT)
				handleWrite(i);
			if (i < _pollFds.size() && (revents & POLLIN))
				handleRead(i);
		}
	}
}

/* --------------------------------- accept --------------------------------- */

void	Server::acceptNewClient(void)
{
	/* Un POLLIN en el socket de escucha puede traer varias conexiones. */
	while (true)
	{
		struct sockaddr_in	addr;
		socklen_t			len = sizeof(addr);
		int					fd = accept(_listenFd,
								reinterpret_cast<struct sockaddr *>(&addr), &len);

		if (fd < 0)
			break ;

		std::string	host = inet_ntoa(addr.sin_addr);

		try
		{
			setNonBlocking(fd);
		}
		catch (const std::exception &e)
		{
			close(fd);
			continue ;
		}
		_clients[fd] = new Client(fd, host);
		addPollFd(fd, POLLIN);
	}
}

/* ---------------------------------- lectura ------------------------------- */

void	Server::dispatchLines(Client &client)
{
	std::string	line;

	while (client.extractLine(line))
		_handler->execute(client, line);
}

void	Server::handleRead(std::size_t index)
{
	int		fd = _pollFds[index].fd;
	Client	*client = _clients[fd];
	char	buf[1024];
	ssize_t	n = recv(fd, buf, sizeof(buf), 0);

	if (n <= 0)
		return (disconnectClient(index, "Connection closed"));

	if (!client->appendToReadBuffer(buf, static_cast<std::size_t>(n)))
		client->markForClose("Input line too long");
	else
		dispatchLines(*client);

	if (client->isMarkedForClose() && !client->hasPendingOutput())
		return (disconnectClient(index, client->getQuitReason()));
	updatePollEvents(index);
}

/* ---------------------------------- escritura ----------------------------- */

void	Server::handleWrite(std::size_t index)
{
	int			fd = _pollFds[index].fd;
	Client		*client = _clients[fd];
	const std::string	&out = client->peekWriteBuffer();

	if (out.empty())
	{
		updatePollEvents(index);
		return ;
	}

	ssize_t	n = send(fd, out.c_str(), out.size(), 0);

	if (n <= 0)
		return (disconnectClient(index, "Write error"));

	client->consumeWriteBuffer(static_cast<std::size_t>(n));
	if (client->isMarkedForClose() && !client->hasPendingOutput())
		return (disconnectClient(index, client->getQuitReason()));
	updatePollEvents(index);
}

/* ----------------------------------- baja --------------------------------- */

void	Server::disconnectClient(std::size_t index, const std::string &reason)
{
	int		fd = _pollFds[index].fd;
	Client	*client = _clients.count(fd) ? _clients[fd] : 0;

	if (client)
	{
		if (!client->isMarkedForClose())
			client->markForClose(reason);
		/* Ultimo intento de vaciar el buffer para que llegue el mensaje
		 * final; no bloqueamos si el socket no acepta mas. */
		if (client->hasPendingOutput())
		{
			const std::string	&out = client->peekWriteBuffer();
			send(fd, out.c_str(), out.size(), 0);
		}
		_handler->onClientDisconnect(*client);
		delete client;
		_clients.erase(fd);
	}

	close(fd);
	_pollFds.erase(_pollFds.begin() + index);
}

/* ----------------------------- IServerContext ----------------------------- */

std::string	Server::toLower(const std::string &s)
{
	std::string	out(s);

	for (std::size_t i = 0; i < out.size(); ++i)
	{
		char	c = out[i];

		if (c >= 'A' && c <= 'Z')
			out[i] = static_cast<char>(c - 'A' + 'a');
	}
	return (out);
}

Client	*Server::findClientByNick(const std::string &nick) const
{
	std::string	target = toLower(nick);
	std::map<int, Client *>::const_iterator	it;

	for (it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (toLower(it->second->getNick()) == target)
			return (it->second);
	}
	return (0);
}

bool	Server::isNickInUse(const std::string &nick) const
{
	return (findClientByNick(nick) != 0);
}

const std::string	&Server::getPassword(void) const
{
	return (_password);
}

const std::string	&Server::getServerName(void) const
{
	return (_name);
}
