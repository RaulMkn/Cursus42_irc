#include <cctype>
#include <cstdlib>
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
	if (client.getState() != Client::PASS_OK)
		return ;
	if (client.getNick().empty() || client.getUser().empty())
		return ;

	client.setState(Client::REGISTERED);
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

Channel	*CommandHandler::getOrCreateChannel(const std::string &name)
{
	Channel	*channel = findChannel(name);

	if (channel)
		return (channel);
	channel = new Channel(name);
	_channels[name] = channel;
	return (channel);
}

void	CommandHandler::removeClientFromChannel(Client &client, Channel &channel,
			const std::string &leaveMessage)
{
	std::string	name = channel.getName();

	channel.broadcast(leaveMessage);
	channel.removeMember(&client);
	client.removeChannel(name);
	if (channel.empty())
	{
		_channels.erase(name);
		delete &channel;
	}
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
	if (client.getState() == Client::HANDSHAKE)
		client.setState(Client::PASS_OK);
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

/* ------------------------- channel membership / messaging -------------------- */

void	CommandHandler::cmdJoin(Client &client, const std::vector<std::string> &params)
{
	if (params.empty())
		return (client.enqueue(numericReply(ERR_NEEDMOREPARAMS, client,
			"JOIN :Not enough parameters")));

	const std::string	&name = params[0];
	std::string			key = params.size() > 1 ? params[1] : "";

	if (name.empty() || name[0] != '#')
		return (client.enqueue(numericReply(ERR_NOSUCHCHANNEL, client, name + " :No such channel")));

	Channel	*existing = findChannel(name);

	if (existing)
	{
		if (existing->isMember(&client))
			return ;
		if (existing->isInviteOnly() && !existing->isInvited(&client))
			return (client.enqueue(numericReply(ERR_INVITEONLYCHAN, client,
				name + " :Cannot join channel (+i)")));
		if (existing->hasKey() && existing->getKey() != key)
			return (client.enqueue(numericReply(ERR_BADCHANNELKEY, client,
				name + " :Cannot join channel (+k)")));
		if (existing->hasLimit() && existing->getMembers().size() >= existing->getLimit())
			return (client.enqueue(numericReply(ERR_CHANNELISFULL, client,
				name + " :Cannot join channel (+l)")));
	}

	bool		isNew = (existing == 0);
	Channel		*channel = getOrCreateChannel(name);
	std::string	announcement = client.prefix() + " JOIN :" + name;

	channel->addMember(&client);
	client.addChannel(name);
	channel->clearInvite(&client);
	if (isNew)
		channel->addOperator(&client);
	channel->broadcast(announcement);

	if (channel->getTopic().empty())
		client.enqueue(numericReply(RPL_NOTOPIC, client, name + " :No topic is set"));
	else
		client.enqueue(numericReply(RPL_TOPIC, client, name + " :" + channel->getTopic()));

	std::string							names;
	const std::set<Client *>			&members = channel->getMembers();
	std::set<Client *>::const_iterator	it;

	for (it = members.begin(); it != members.end(); ++it)
	{
		if (!names.empty())
			names += " ";
		if (channel->isOperator(*it))
			names += "@";
		names += (*it)->getNick();
	}
	client.enqueue(numericReply(RPL_NAMREPLY, client, "= " + name + " :" + names));
	client.enqueue(numericReply(RPL_ENDOFNAMES, client, name + " :End of /NAMES list"));
}

void	CommandHandler::cmdPart(Client &client, const std::vector<std::string> &params)
{
	if (params.empty())
		return (client.enqueue(numericReply(ERR_NEEDMOREPARAMS, client,
			"PART :Not enough parameters")));

	Channel	*channel = findChannel(params[0]);

	if (!channel)
		return (client.enqueue(numericReply(ERR_NOSUCHCHANNEL, client,
			params[0] + " :No such channel")));
	if (!channel->isMember(&client))
		return (client.enqueue(numericReply(ERR_NOTONCHANNEL, client,
			params[0] + " :You're not on that channel")));

	std::string	reason = params.size() > 1 ? params[1] : client.getNick();
	std::string	announcement = client.prefix() + " PART " + params[0] + " :" + reason;

	removeClientFromChannel(client, *channel, announcement);
}

void	CommandHandler::cmdPrivmsg(Client &client, const std::vector<std::string> &params)
{
	if (params.empty())
		return (client.enqueue(numericReply(ERR_NORECIPIENT, client,
			":No recipient given (PRIVMSG)")));
	if (params.size() < 2 || params[1].empty())
		return (client.enqueue(numericReply(ERR_NOTEXTTOSEND, client, ":No text to send")));

	const std::string	&target = params[0];
	const std::string	&text = params[1];
	std::string			line = client.prefix() + " PRIVMSG " + target + " :" + text;

	if (target[0] == '#')
	{
		Channel	*channel = findChannel(target);

		if (!channel)
			return (client.enqueue(numericReply(ERR_NOSUCHCHANNEL, client,
				target + " :No such channel")));
		if (!channel->isMember(&client))
			return (client.enqueue(numericReply(ERR_CANNOTSENDTOCHAN, client,
				target + " :Cannot send to channel")));
		return (channel->broadcast(line, &client));
	}

	Client	*recipient = _context.findClientByNick(target);

	if (!recipient)
		return (client.enqueue(numericReply(ERR_NOSUCHNICK, client,
			target + " :No such nick/channel")));
	recipient->enqueue(line);
}

void	CommandHandler::cmdNotice(Client &client, const std::vector<std::string> &params)
{
	if (params.size() < 2 || params[0].empty() || params[1].empty())
		return ;

	const std::string	&target = params[0];
	const std::string	&text = params[1];
	std::string			line = client.prefix() + " NOTICE " + target + " :" + text;

	if (target[0] == '#')
	{
		Channel	*channel = findChannel(target);

		if (channel && channel->isMember(&client))
			channel->broadcast(line, &client);
		return ;
	}

	Client	*recipient = _context.findClientByNick(target);

	if (recipient)
		recipient->enqueue(line);
}

/* ------------------------------- operator commands ---------------------------- */

void	CommandHandler::cmdTopic(Client &client, const std::vector<std::string> &params)
{
	if (params.empty())
		return (client.enqueue(numericReply(ERR_NEEDMOREPARAMS, client,
			"TOPIC :Not enough parameters")));

	Channel	*channel = findChannel(params[0]);

	if (!channel)
		return (client.enqueue(numericReply(ERR_NOSUCHCHANNEL, client,
			params[0] + " :No such channel")));
	if (!channel->isMember(&client))
		return (client.enqueue(numericReply(ERR_NOTONCHANNEL, client,
			params[0] + " :You're not on that channel")));

	if (params.size() < 2)
	{
		if (channel->getTopic().empty())
			return (client.enqueue(numericReply(RPL_NOTOPIC, client,
				params[0] + " :No topic is set")));
		return (client.enqueue(numericReply(RPL_TOPIC, client,
			params[0] + " :" + channel->getTopic())));
	}

	if (channel->isTopicRestricted() && !channel->isOperator(&client))
		return (client.enqueue(numericReply(ERR_CHANOPRIVSNEEDED, client,
			params[0] + " :You're not channel operator")));

	channel->setTopic(params[1]);
	channel->broadcast(client.prefix() + " TOPIC " + params[0] + " :" + params[1]);
}

void	CommandHandler::cmdKick(Client &client, const std::vector<std::string> &params)
{
	if (params.size() < 2)
		return (client.enqueue(numericReply(ERR_NEEDMOREPARAMS, client,
			"KICK :Not enough parameters")));

	Channel	*channel = findChannel(params[0]);

	if (!channel)
		return (client.enqueue(numericReply(ERR_NOSUCHCHANNEL, client,
			params[0] + " :No such channel")));
	if (!channel->isMember(&client))
		return (client.enqueue(numericReply(ERR_NOTONCHANNEL, client,
			params[0] + " :You're not on that channel")));
	if (!channel->isOperator(&client))
		return (client.enqueue(numericReply(ERR_CHANOPRIVSNEEDED, client,
			params[0] + " :You're not channel operator")));

	Client	*target = _context.findClientByNick(params[1]);

	if (!target || !channel->isMember(target))
		return (client.enqueue(numericReply(ERR_USERNOTINCHANNEL, client,
			params[1] + " " + params[0] + " :They aren't on that channel")));

	std::string	reason = params.size() > 2 ? params[2] : client.getNick();
	std::string	announcement = client.prefix() + " KICK " + params[0] + " "
		+ params[1] + " :" + reason;

	removeClientFromChannel(*target, *channel, announcement);
}

void	CommandHandler::cmdInvite(Client &client, const std::vector<std::string> &params)
{
	if (params.size() < 2)
		return (client.enqueue(numericReply(ERR_NEEDMOREPARAMS, client,
			"INVITE :Not enough parameters")));

	Client	*target = _context.findClientByNick(params[0]);

	if (!target)
		return (client.enqueue(numericReply(ERR_NOSUCHNICK, client,
			params[0] + " :No such nick/channel")));

	Channel	*channel = findChannel(params[1]);

	if (!channel)
		return (client.enqueue(numericReply(ERR_NOSUCHCHANNEL, client,
			params[1] + " :No such channel")));
	if (!channel->isMember(&client))
		return (client.enqueue(numericReply(ERR_NOTONCHANNEL, client,
			params[1] + " :You're not on that channel")));
	if (channel->isInviteOnly() && !channel->isOperator(&client))
		return (client.enqueue(numericReply(ERR_CHANOPRIVSNEEDED, client,
			params[1] + " :You're not channel operator")));
	if (channel->isMember(target))
		return (client.enqueue(numericReply(ERR_USERONCHANNEL, client,
			params[0] + " " + params[1] + " :is already on channel")));

	channel->invite(target);
	client.enqueue(numericReply(RPL_INVITING, client, params[0] + " " + params[1]));
	target->enqueue(client.prefix() + " INVITE " + params[0] + " :" + params[1]);
}

void	CommandHandler::cmdMode(Client &client, const std::vector<std::string> &params)
{
	if (params.size() < 2)
		return (client.enqueue(numericReply(ERR_NEEDMOREPARAMS, client,
			"MODE :Not enough parameters")));

	Channel	*channel = findChannel(params[0]);

	if (!channel)
		return (client.enqueue(numericReply(ERR_NOSUCHCHANNEL, client,
			params[0] + " :No such channel")));
	if (!channel->isOperator(&client))
		return (client.enqueue(numericReply(ERR_CHANOPRIVSNEEDED, client,
			params[0] + " :You're not channel operator")));

	const std::string	&modes = params[1];
	std::size_t			argIndex = 2;
	bool				adding = true;
	std::string			appliedFlags;
	std::string			appliedArgs;

	for (std::size_t i = 0; i < modes.size(); ++i)
	{
		char	flag = modes[i];

		if (flag == '+')
		{
			adding = true;
			continue ;
		}
		if (flag == '-')
		{
			adding = false;
			continue ;
		}
		if (flag == 'i')
		{
			channel->setInviteOnly(adding);
			appliedFlags += adding ? "+i" : "-i";
		}
		else if (flag == 't')
		{
			channel->setTopicRestricted(adding);
			appliedFlags += adding ? "+t" : "-t";
		}
		else if (flag == 'k')
		{
			if (adding)
			{
				if (argIndex >= params.size())
					continue ;
				channel->setKey(params[argIndex]);
				appliedArgs += " " + params[argIndex];
				++argIndex;
			}
			else
				channel->clearKey();
			appliedFlags += adding ? "+k" : "-k";
		}
		else if (flag == 'l')
		{
			if (adding)
			{
				if (argIndex >= params.size())
					continue ;
				channel->setLimit(static_cast<std::size_t>(std::atoi(params[argIndex].c_str())));
				appliedArgs += " " + params[argIndex];
				++argIndex;
			}
			else
				channel->clearLimit();
			appliedFlags += adding ? "+l" : "-l";
		}
		else if (flag == 'o')
		{
			if (argIndex >= params.size())
				continue ;

			Client	*target = _context.findClientByNick(params[argIndex]);

			if (target && channel->isMember(target))
			{
				if (adding)
					channel->addOperator(target);
				else
					channel->removeOperator(target);
				appliedFlags += adding ? "+o" : "-o";
				appliedArgs += " " + params[argIndex];
			}
			++argIndex;
		}
		else
			client.enqueue(numericReply(ERR_UNKNOWNMODE, client,
				std::string(1, flag) + " :is unknown mode char to me"));
	}

	if (!appliedFlags.empty())
		channel->broadcast(client.prefix() + " MODE " + params[0] + " "
			+ appliedFlags + appliedArgs);
}

/* ---------------------------------- disconnect --------------------------------- */

void	CommandHandler::onClientDisconnect(Client &client)
{
	std::set<std::string>					channels = client.getChannels();
	std::set<std::string>::const_iterator	it;
	std::string								announcement = client.prefix()
		+ " QUIT :" + client.getQuitReason();

	for (it = channels.begin(); it != channels.end(); ++it)
	{
		Channel	*channel = findChannel(*it);

		if (channel)
			removeClientFromChannel(client, *channel, announcement);
	}
}
