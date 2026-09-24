/* ************************************************************************** */
/*                                                                            */
/*   Server.hpp                                                               */
/*                                                                            */
/*   PARTE 1 - Red y core del servidor.                                       */
/*   Es la duena de los objetos Client (los crea en accept y los destruye en  */
/*   la baja) y del unico bucle poll(). Implementa IServerContext para que    */
/*   la Parte 2 (CommandHandler) pueda buscar clientes por nick y consultar   */
/*   datos del servidor sin conocer nada de sockets.                          */
/*                                                                            */
/*   No interpreta ni un byte de IRC: reconstruye lineas y las entrega a      */
/*   ICommandHandler::execute. Ver docs/client-contract.md.                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
# define SERVER_HPP

# include <map>
# include <string>
# include <vector>
# include <poll.h>

# include "IServerContext.hpp"

class Client;
class ICommandHandler;

class Server : public IServerContext
{
	public:
		Server(unsigned short port, const std::string &password,
			const std::string &name = "irc.42madrid");
		~Server(void);

		/* Enlaza al handler de la Parte 2 y arranca el bucle poll().
		 * Solo vuelve cuando se recibe una senal de parada. */
		void	setCommandHandler(ICommandHandler &handler);
		void	run(void);

		/* Pide una parada ordenada del bucle (lo usa el handler de senales). */
		static void	requestStop(void);

		/* ---- IServerContext ---- */
		Client	*findClientByNick(const std::string &nick) const;
		bool	isNickInUse(const std::string &nick) const;
		const std::string	&getPassword(void) const;
		const std::string	&getServerName(void) const;

	private:
		Server(const Server &other);
		Server	&operator=(const Server &other);

		/* ---- arranque ---- */
		void	setupListenSocket(void);
		void	setNonBlocking(int fd);

		/* ---- eventos de poll ---- */
		void	acceptNewClient(void);
		void	handleRead(std::size_t index);
		void	handleWrite(std::size_t index);

		/* ---- gestion de fds/clientes ---- */
		void	addPollFd(int fd, short events);
		void	updatePollEvents(std::size_t index);
		void	disconnectClient(std::size_t index, const std::string &reason);
		void	dispatchLines(Client &client);

		/* ---- helpers ---- */
		static std::string	toLower(const std::string &s);

		unsigned short			_port;
		std::string				_password;
		std::string				_name;
		int						_listenFd;

		ICommandHandler			*_handler;
		std::vector<struct pollfd>		_pollFds;
		std::map<int, Client *>			_clients;

		static volatile bool	_running;
};

#endif
