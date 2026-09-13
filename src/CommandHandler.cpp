#include <cctype>
#include <set>
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

/* --------------------------- registration helpers --------------------------- */

bool	CommandHandler::isValidNick(const std::string &nick) const
{
	static const std::string	special = "-_[]{}\\`^|";

	if (nick.empty() || nick.size() > 9)
		return (false);
	if (std::isdigit(static_cast<unsigned char>(nick[0])))
		return (false);
	for (std::size_t i = 0; i < nick.size(); ++i)
	{
		if (!std::isalnum(static_cast<unsigned char>(nick[i]))
			&& special.find(nick[i]) == std::string::npos)
			return (false);
	}
	return (true);
}

void	CommandHandler::maybeCompleteRegistration(Client &client)
{
	if (client.getState() != PASS_OK)
		return ;
	if (client.getNick().empty() || client.getUser().empty())
		return ;

	client.setState(REGISTERED);
	client.enqueue(numericReply(RPL_WELCOME, client,
		":Welcome to the Internet Relay Network " + client.prefix()));
	client.enqueue(numericReply(RPL_YOURHOST, client,
		":Your host is " + _context.getServerName() + ", running ft_irc"));
	client.enqueue(numericReply(RPL_CREATED, client, ":This server was created today"));
	client.enqueue(numericReply(RPL_MYINFO, client,
		_context.getServerName() + " ft_irc-1.0 o itkol"));
}

/* ------------------------------ helpers: channels ---------------------------- */

Channel	*CommandHandler::findChannel(const std::string &name) const
{
	std::map<std::string, Channel *>::const_iterator	it = _channels.find(name);

	if (it == _channels.end())
		return (0);
	return (it->second);
}

/* ------------------------------- registration -------------------------------- */

void	CommandHandler::cmdPass(Client &client, const std::vector<std::string> &params)
{
	if (client.isRegistered())
		return (client.enqueue(numericReply(ERR_ALREADYREGISTRED, client,
			":Unauthorized command (already registered)")));
	if (params.empty())
		return (client.enqueue(numericReply(ERR_NEEDMOREPARAMS, client,
			"PASS :Not enough parameters")));
	if (params[0] != _context.getPassword())
	{
		client.enqueue(numericReply(ERR_PASSWDMISMATCH, client, ":Password incorrect"));
		return (client.markForClose("Bad password"));
	}
	if (client.getState() == HANDSHAKE)
		client.setState(PASS_OK);
}

void	CommandHandler::cmdNick(Client &client, const std::vector<std::string> &params)
{
	if (params.empty() || params[0].empty())
		return (client.enqueue(numericReply(ERR_NONICKNAMEGIVEN, client, ":No nickname given")));

	const std::string	&nick = params[0];

	if (!isValidNick(nick))
		return (client.enqueue(numericReply(ERR_ERRONEUSNICKNAME, client,
			nick + " :Erroneous nickname")));
	if (_context.isNickInUse(nick))
		return (client.enqueue(numericReply(ERR_NICKNAMEINUSE, client,
			nick + " :Nickname is already in use")));

	if (client.isRegistered())
	{
		std::string	announcement = client.prefix() + " NICK :" + nick;
		const std::set<std::string>	&channels = client.getChannels();
		std::set<std::string>::const_iterator	it;

		client.setNick(nick);
		client.enqueue(announcement);
		for (it = channels.begin(); it != channels.end(); ++it)
		{
			Channel	*channel = findChannel(*it);

			if (channel)
				channel->broadcast(announcement, &client);
		}
	}
	else
	{
		client.setNick(nick);
		maybeCompleteRegistration(client);
	}
}

void	CommandHandler::cmdUser(Client &client, const std::vector<std::string> &params)
{
	if (client.isRegistered())
		return (client.enqueue(numericReply(ERR_ALREADYREGISTRED, client,
			":Unauthorized command (already registered)")));
	if (params.size() < 4)
		return (client.enqueue(numericReply(ERR_NEEDMOREPARAMS, client,
			"USER :Not enough parameters")));

	client.setUser(params[0]);
	client.setRealname(params[3]);
	maybeCompleteRegistration(client);
}

void	CommandHandler::cmdCap(Client &client, const std::vector<std::string> &params)
{
	if (!params.empty() && params[0] == "LS")
		client.enqueue("CAP * LS :");
}

void	CommandHandler::cmdPing(Client &client, const std::vector<std::string> &params)
{
	std::string	token = params.empty() ? _context.getServerName() : params[0];

	client.enqueue(":" + _context.getServerName() + " PONG "
		+ _context.getServerName() + " :" + token);
}

void	CommandHandler::cmdQuit(Client &client, const std::vector<std::string> &params)
{
	std::string	reason = params.empty() ? "Client Quit" : params[0];

	client.markForClose(reason);
}
