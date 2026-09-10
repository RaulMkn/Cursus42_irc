/* ************************************************************************** */
/*                                                                            */
/*   Client.hpp                                                               */
/*                                                                            */
/*   Objeto compartido entre la Parte 1 (red) y la Parte 2 (protocolo).       */
/*   Es la UNICA estructura que ambos modulos conocen. Ver el contrato de     */
/*   escritura/lectura en docs/client-contract.md antes de tocar nada.        */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <cstddef>
# include <set>
# include <string>

class Client
{
	public:
		/* Maquina de estados del registro (RFC 1459, seccion 4.1).
		 * Solo la Parte 2 hace transiciones. La Parte 1 solo lee. */
		enum State
		{
			HANDSHAKE,  /* recien aceptado: nada validado             */
			PASS_OK,    /* PASS correcto: faltan NICK y/o USER        */
			REGISTERED  /* NICK + USER completos: acceso a todo       */
		};

		/* Limite de un mensaje IRC incluyendo el CRLF final. */
		static const std::size_t	MAX_MESSAGE_LEN = 512;

		Client(int fd, const std::string &host);
		~Client(void);

		/* ---- Identidad de red: la fija el constructor, nadie la muta ---- */
		int					getFd(void) const;
		const std::string	&getHost(void) const;

		/* ---- Estado logico: escribe la Parte 2, lee quien quiera ---- */
		State				getState(void) const;
		void				setState(State state);
		bool				isRegistered(void) const;

		const std::string	&getNick(void) const;
		void				setNick(const std::string &nick);
		const std::string	&getUser(void) const;
		void				setUser(const std::string &user);
		const std::string	&getRealname(void) const;
		void				setRealname(const std::string &realname);

		/* Prefijo "nick!user@host" para reenviar mensajes.
		 * Devuelve "*" como nick mientras no este registrado. */
		std::string			prefix(void) const;

		/* ---- Canales: los mantiene la Parte 2. La Parte 1 los recorre
		 * al desconectar para poder avisar (QUIT) antes de liberar. ---- */
		const std::set<std::string>	&getChannels(void) const;
		void						addChannel(const std::string &name);
		void						removeChannel(const std::string &name);
		bool						isInChannel(const std::string &name) const;

		/* ---- Buffer de entrada: EXCLUSIVO de la Parte 1 ----
		 * appendToReadBuffer devuelve false si la linea pendiente supera
		 * MAX_MESSAGE_LEN sin CRLF (cliente malicioso o roto): en ese caso
		 * el buffer se descarta y hay que llamar a markForClose.
		 * extractLine saca el siguiente comando completo sin el CRLF y
		 * descarta las lineas vacias, que el RFC obliga a ignorar. */
		bool	appendToReadBuffer(const char *data, std::size_t len);
		bool	extractLine(std::string &out);

		/* ---- Buffer de salida ----
		 * enqueue lo usa la Parte 2 (anade el CRLF si falta).
		 * El resto lo usa la Parte 1 en el ciclo de poll(). */
		void				enqueue(const std::string &msg);
		bool				hasPendingOutput(void) const;
		const std::string	&peekWriteBuffer(void) const;
		void				consumeWriteBuffer(std::size_t n);

		/* ---- Cierre ordenado ----
		 * Ningun modulo llama a close() salvo la Parte 1. La Parte 2 pide
		 * la desconexion con markForClose y la red la ejecuta cuando ha
		 * terminado de vaciar el buffer de salida. */
		void				markForClose(const std::string &reason);
		bool				isMarkedForClose(void) const;
		const std::string	&getQuitReason(void) const;

	private:
		Client(const Client &other);
		Client	&operator=(const Client &other);

		const int				_fd;
		const std::string		_host;

		State					_state;
		std::string				_nick;
		std::string				_user;
		std::string				_realname;
		std::set<std::string>	_channels;

		std::string				_readBuffer;
		std::string				_writeBuffer;

		bool					_markedForClose;
		std::string				_quitReason;
};

#endif
