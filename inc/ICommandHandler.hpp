/* ************************************************************************** */
/*                                                                            */
/*   ICommandHandler.hpp                                                      */
/*                                                                            */
/*   Seam PARTE 1 -> PARTE 2.                                                 */
/*   La red no sabe nada de IRC: cuando reconstruye un comando completo lo    */
/*   entrega aqui. La Parte 2 implementa esta interfaz (CommandHandler) y la  */
/*   Parte 1 solo guarda un puntero a ICommandHandler, asi los dos modulos    */
/*   compilan por separado y se puede trabajar con un stub.                   */
/*                                                                            */
/* ************************************************************************** */

#ifndef ICOMMANDHANDLER_HPP
# define ICOMMANDHANDLER_HPP

# include <string>

class Client;

class ICommandHandler
{
	public:
		virtual ~ICommandHandler(void) {}

		/* Un comando completo, ya sin CRLF y garantizado no vacio.
		 * La Parte 2 parsea, ejecuta y responde con client.enqueue() o con
		 * otro->enqueue() si tiene que retransmitir a un canal. Nunca llama
		 * a send(), recv() ni close(). */
		virtual void	execute(Client &client, const std::string &line) = 0;

		/* La red avisa ANTES de destruir el objeto Client. Es obligatorio:
		 * aqui la Parte 2 propaga el QUIT a los canales y borra el puntero
		 * de todas sus estructuras. Si no, quedan punteros colgando. */
		virtual void	onClientDisconnect(Client &client) = 0;
};

#endif
