/* ************************************************************************** */
/*                                                                            */
/*   IServerContext.hpp                                                       */
/*                                                                            */
/*   Seam PARTE 2 -> PARTE 1.                                                 */
/*   La Parte 1 es la duena de los objetos Client (los crea y los destruye),   */
/*   asi que es la unica que tiene el registro fd -> Client*. La Parte 2       */
/*   necesita buscar por nick (PRIVMSG, KICK, INVITE) y consultar datos del    */
/*   servidor; lo hace solo a traves de esta interfaz.                         */
/*                                                                            */
/*   Los punteros devueltos son PRESTADOS: validos durante la ejecucion del    */
/*   comando en curso y nunca liberables por la Parte 2.                       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ISERVERCONTEXT_HPP
# define ISERVERCONTEXT_HPP

# include <string>

class Client;

class IServerContext
{
	public:
		virtual ~IServerContext(void) {}

		/* NULL si no hay ningun cliente registrado con ese nick.
		 * La comparacion es case-insensitive, como exige el RFC. */
		virtual Client	*findClientByNick(const std::string &nick) const = 0;
		virtual bool	isNickInUse(const std::string &nick) const = 0;

		/* Password del servidor (argv[2]) para validar PASS. */
		virtual const std::string	&getPassword(void) const = 0;

		/* Nombre del servidor: prefijo de todas las respuestas numericas
		 * (":irc.42madrid 001 nick :Welcome..."). */
		virtual const std::string	&getServerName(void) const = 0;
};

#endif
