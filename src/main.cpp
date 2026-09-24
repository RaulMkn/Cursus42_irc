/* ************************************************************************** */
/*                                                                            */
/*   main.cpp                                                                 */
/*                                                                            */
/*   Punto de entrada. Valida los argumentos (./ircserv <port> <password>),    */
/*   instala los manejadores de senal y arranca el Server con su              */
/*   CommandHandler. Nada de logica de red ni de protocolo vive aqui.          */
/*                                                                            */
/* ************************************************************************** */

#include <cstdlib>
#include <iostream>
#include <string>
#include <csignal>

#include "Server.hpp"
#include "CommandHandler.hpp"

static void	handleSignal(int)
{
	Server::requestStop();
}

/* Puerto valido: entero 1..65535, sin caracteres sobrantes. */
static bool	parsePort(const std::string &arg, unsigned short &port)
{
	if (arg.empty())
		return (false);
	for (std::size_t i = 0; i < arg.size(); ++i)
	{
		if (arg[i] < '0' || arg[i] > '9')
			return (false);
	}

	long	value = std::strtol(arg.c_str(), 0, 10);

	if (value < 1 || value > 65535)
		return (false);
	port = static_cast<unsigned short>(value);
	return (true);
}

int	main(int argc, char **argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
		return (1);
	}

	unsigned short	port;

	if (!parsePort(argv[1], port))
	{
		std::cerr << "Error: port must be an integer in [1, 65535]" << std::endl;
		return (1);
	}

	std::string	password(argv[2]);

	if (password.empty())
	{
		std::cerr << "Error: password must not be empty" << std::endl;
		return (1);
	}

	/* SIGINT/SIGTERM: parada ordenada. SIGPIPE: ignorado para no morir al
	 * escribir en un socket que el cliente acaba de cerrar. */
	std::signal(SIGINT, handleSignal);
	std::signal(SIGTERM, handleSignal);
	std::signal(SIGPIPE, SIG_IGN);

	try
	{
		Server			server(port, password);
		CommandHandler	handler(server);

		server.setCommandHandler(handler);
		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Fatal: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}
