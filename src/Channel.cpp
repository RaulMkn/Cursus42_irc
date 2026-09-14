#include "Channel.hpp"
#include "Client.hpp"

Channel::Channel(const std::string &name) :
	_name(name),
	_inviteOnly(false),
	_topicRestricted(false),
	_hasKey(false),
	_hasLimit(false),
	_limit(0)
{
}

Channel::~Channel(void)
{
}

const std::string	&Channel::getName(void) const
{
	return (_name);
}

/* ------------------------------ membership -------------------------------- */

void	Channel::addMember(Client *client)
{
	_members.insert(client);
}

void	Channel::removeMember(Client *client)
{
	_members.erase(client);
	_operators.erase(client);
}

bool	Channel::isMember(Client *client) const
{
	return (_members.find(client) != _members.end());
}

bool	Channel::empty(void) const
{
	return (_members.empty());
}

const std::set<Client *>	&Channel::getMembers(void) const
{
	return (_members);
}

/* ------------------------------- operators --------------------------------- */

void	Channel::addOperator(Client *client)
{
	_operators.insert(client);
}

void	Channel::removeOperator(Client *client)
{
	_operators.erase(client);
}

bool	Channel::isOperator(Client *client) const
{
	return (_operators.find(client) != _operators.end());
}

/* --------------------------------- invites ---------------------------------- */

void	Channel::invite(Client *client)
{
	_invited.insert(client);
}

bool	Channel::isInvited(Client *client) const
{
	return (_invited.find(client) != _invited.end());
}

void	Channel::clearInvite(Client *client)
{
	_invited.erase(client);
}

/* ---------------------------------- topic ------------------------------------ */

const std::string	&Channel::getTopic(void) const
{
	return (_topic);
}

void	Channel::setTopic(const std::string &topic)
{
	_topic = topic;
}

/* ---------------------------------- modes ------------------------------------ */

bool	Channel::isInviteOnly(void) const
{
	return (_inviteOnly);
}

void	Channel::setInviteOnly(bool value)
{
	_inviteOnly = value;
}

bool	Channel::isTopicRestricted(void) const
{
	return (_topicRestricted);
}

void	Channel::setTopicRestricted(bool value)
{
	_topicRestricted = value;
}

bool	Channel::hasKey(void) const
{
	return (_hasKey);
}

const std::string	&Channel::getKey(void) const
{
	return (_key);
}

void	Channel::setKey(const std::string &key)
{
	_hasKey = true;
	_key = key;
}

void	Channel::clearKey(void)
{
	_hasKey = false;
	_key.clear();
}

bool	Channel::hasLimit(void) const
{
	return (_hasLimit);
}

std::size_t	Channel::getLimit(void) const
{
	return (_limit);
}

void	Channel::setLimit(std::size_t limit)
{
	_hasLimit = true;
	_limit = limit;
}

void	Channel::clearLimit(void)
{
	_hasLimit = false;
	_limit = 0;
}

/* -------------------------------- broadcast ----------------------------------- */

void	Channel::broadcast(const std::string &msg, Client *exclude) const
{
	std::set<Client *>::const_iterator	it;

	for (it = _members.begin(); it != _members.end(); ++it)
	{
		if (*it != exclude)
			(*it)->enqueue(msg);
	}
}
