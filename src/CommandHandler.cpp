#include <cctype>
#include <sstream>

#include "CommandHandler.hpp"
#include "Channel.hpp"
#include "Client.hpp"
#include "IServerContext.hpp"
#include "Numerics.hpp"

CommandHandler::CommandHandler(IServerContext &context) : _context(context)
{
}

CommandHandler::~CommandHandler(void)
{
	std::map<std::string, Channel *>::iterator	it;

	for (it = _channels.begin(); it != _channels.end(); ++it)
		delete it->second;
}

/* -------------------------------- parsing --------------------------------- */

void	CommandHandler::tokenize(const std::string &line, std::string &command,
			std::vector<std::string> &params) const
{
	std::string::size_type	pos = 0;
	std::string::size_type	len = line.size();
	std::string::size_type	start;

	command.clear();
	params.clear();

	while (pos < len && line[pos] == ' ')
		++pos;
	start = pos;
	while (pos < len && line[pos] != ' ')
		++pos;
	command = line.substr(start, pos - start);
	for (std::string::size_type i = 0; i < command.size(); ++i)
		command[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(command[i])));

	while (pos < len)
	{
		while (pos < len && line[pos] == ' ')
			++pos;
		if (pos >= len)
			break;
		if (line[pos] == ':')
		{
			params.push_back(line.substr(pos + 1));
			break;
		}
		start = pos;
		while (pos < len && line[pos] != ' ')
			++pos;
		params.push_back(line.substr(start, pos - start));
	}
}

/* ------------------------------ reply helper ------------------------------- */

std::string	CommandHandler::numericReply(int code, const Client &client,
				const std::string &text) const
{
	std::ostringstream	oss;
	std::string			codeStr;
	std::string			target;

	oss << code;
	codeStr = oss.str();
	while (codeStr.size() < 3)
		codeStr = "0" + codeStr;
	target = client.getNick().empty() ? "*" : client.getNick();
	return (":" + _context.getServerName() + " " + codeStr + " " + target + " " + text);
}

/* --------------------------------- execute --------------------------------- */

void	CommandHandler::execute(Client &client, const std::string &line)
{
	std::string					command;
	std::vector<std::string>	params;

	tokenize(line, command, params);
	if (command.empty())
		return ;

	if (command == "PASS")
		cmdPass(client, params);
	else if (command == "NICK")
		cmdNick(client, params);
	else if (command == "USER")
		cmdUser(client, params);
	else if (command == "CAP")
		cmdCap(client, params);
	else if (command == "PING")
		cmdPing(client, params);
	else if (command == "QUIT")
		cmdQuit(client, params);
	else if (!client.isRegistered())
		client.enqueue(numericReply(ERR_NOTREGISTERED, client, ":You have not registered"));
	else if (command == "JOIN")
		cmdJoin(client, params);
	else if (command == "PART")
		cmdPart(client, params);
	else if (command == "PRIVMSG")
		cmdPrivmsg(client, params);
	else if (command == "NOTICE")
		cmdNotice(client, params);
	else if (command == "TOPIC")
		cmdTopic(client, params);
	else if (command == "KICK")
		cmdKick(client, params);
	else if (command == "INVITE")
		cmdInvite(client, params);
	else if (command == "MODE")
		cmdMode(client, params);
	else
		client.enqueue(numericReply(ERR_UNKNOWNCOMMAND, client, command + " :Unknown command"));
}
