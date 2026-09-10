/* ************************************************************************** */
/*                                                                            */
/*   Client.cpp                                                               */
/*                                                                            */
/*   Implementacion del objeto compartido. Aqui solo hay mecanica de estado    */
/*   y de buffers: ni una sola llamada a la API de sockets ni una sola regla   */
/*   del protocolo IRC. Eso vive en la Parte 1 y en la Parte 2.                */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

const std::size_t	Client::MAX_MESSAGE_LEN;

Client::Client(int fd, const std::string &host) :
	_fd(fd),
	_host(host),
	_state(HANDSHAKE),
	_markedForClose(false)
{
}

Client::~Client(void)
{
}

/* ------------------------------ identidad -------------------------------- */

int	Client::getFd(void) const
{
	return (_fd);
}

const std::string	&Client::getHost(void) const
{
	return (_host);
}

/* ---------------------------- estado logico ------------------------------ */

Client::State	Client::getState(void) const
{
	return (_state);
}

void	Client::setState(State state)
{
	_state = state;
}

bool	Client::isRegistered(void) const
{
	return (_state == REGISTERED);
}

const std::string	&Client::getNick(void) const
{
	return (_nick);
}

void	Client::setNick(const std::string &nick)
{
	_nick = nick;
}

const std::string	&Client::getUser(void) const
{
	return (_user);
}

void	Client::setUser(const std::string &user)
{
	_user = user;
}

const std::string	&Client::getRealname(void) const
{
	return (_realname);
}

void	Client::setRealname(const std::string &realname)
{
	_realname = realname;
}

std::string	Client::prefix(void) const
{
	std::string	out;

	out = _nick.empty() ? "*" : _nick;
	out += "!";
	out += _user.empty() ? "unknown" : _user;
	out += "@";
	out += _host;
	return (out);
}

/* ------------------------------- canales --------------------------------- */

const std::set<std::string>	&Client::getChannels(void) const
{
	return (_channels);
}

void	Client::addChannel(const std::string &name)
{
	_channels.insert(name);
}

void	Client::removeChannel(const std::string &name)
{
	_channels.erase(name);
}

bool	Client::isInChannel(const std::string &name) const
{
	return (_channels.find(name) != _channels.end());
}

/* --------------------------- buffer de entrada --------------------------- */

bool	Client::appendToReadBuffer(const char *data, std::size_t len)
{
	_readBuffer.append(data, len);
	if (_readBuffer.find('\n') == std::string::npos
		&& _readBuffer.size() > MAX_MESSAGE_LEN)
	{
		_readBuffer.clear();
		return (false);
	}
	return (true);
}

bool	Client::extractLine(std::string &out)
{
	std::string::size_type	pos;

	while ((pos = _readBuffer.find('\n')) != std::string::npos)
	{
		out.assign(_readBuffer, 0, pos);
		_readBuffer.erase(0, pos + 1);
		if (!out.empty() && out[out.size() - 1] == '\r')
			out.erase(out.size() - 1);
		if (!out.empty())
			return (true);
	}
	out.clear();
	return (false);
}

/* --------------------------- buffer de salida ---------------------------- */

void	Client::enqueue(const std::string &msg)
{
	_writeBuffer += msg;
	if (msg.size() < 2 || msg.compare(msg.size() - 2, 2, "\r\n") != 0)
		_writeBuffer += "\r\n";
}

bool	Client::hasPendingOutput(void) const
{
	return (!_writeBuffer.empty());
}

const std::string	&Client::peekWriteBuffer(void) const
{
	return (_writeBuffer);
}

void	Client::consumeWriteBuffer(std::size_t n)
{
	if (n >= _writeBuffer.size())
		_writeBuffer.clear();
	else
		_writeBuffer.erase(0, n);
}

/* ---------------------------- cierre ordenado ---------------------------- */

void	Client::markForClose(const std::string &reason)
{
	if (_markedForClose)
		return ;
	_markedForClose = true;
	_quitReason = reason;
}

bool	Client::isMarkedForClose(void) const
{
	return (_markedForClose);
}

const std::string	&Client::getQuitReason(void) const
{
	return (_quitReason);
}
