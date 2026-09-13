#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <set>
# include <string>

class Client;

class Channel
{
	public:
		explicit Channel(const std::string &name);
		~Channel(void);

		const std::string	&getName(void) const;

		/* ---- membership ---- */
		void	addMember(Client *client);
		void	removeMember(Client *client);
		bool	isMember(Client *client) const;
		bool	empty(void) const;
		const std::set<Client *>	&getMembers(void) const;

		/* ---- operators (subset of members) ---- */
		void	addOperator(Client *client);
		void	removeOperator(Client *client);
		bool	isOperator(Client *client) const;

		/* ---- invite list, only relevant when mode +i is set ---- */
		void	invite(Client *client);
		bool	isInvited(Client *client) const;
		void	clearInvite(Client *client);

		/* ---- topic ---- */
		const std::string	&getTopic(void) const;
		void				setTopic(const std::string &topic);

		/* ---- modes: i / t / k / l ---- */
		bool	isInviteOnly(void) const;
		void	setInviteOnly(bool value);

		bool	isTopicRestricted(void) const;
		void	setTopicRestricted(bool value);

		bool				hasKey(void) const;
		const std::string	&getKey(void) const;
		void				setKey(const std::string &key);
		void				clearKey(void);

		bool		hasLimit(void) const;
		std::size_t	getLimit(void) const;
		void		setLimit(std::size_t limit);
		void		clearLimit(void);

		/* ---- broadcast ----
		 * Sends msg to every member. If exclude != NULL, that member is
		 * skipped (used by PRIVMSG, which doesn't echo to the sender). */
		void	broadcast(const std::string &msg, Client *exclude = 0) const;

	private:
		Channel(const Channel &other);
		Channel	&operator=(const Channel &other);

		std::string			_name;
		std::string			_topic;
		std::set<Client *>	_members;
		std::set<Client *>	_operators;
		std::set<Client *>	_invited;

		bool				_inviteOnly;
		bool				_topicRestricted;
		bool				_hasKey;
		std::string			_key;
		bool				_hasLimit;
		std::size_t			_limit;
};

#endif
