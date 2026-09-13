#ifndef COMMANDHANDLER_HPP
# define COMMANDHANDLER_HPP

# include <map>
# include <string>
# include <vector>

# include "ICommandHandler.hpp"

class Channel;
class IServerContext;

class CommandHandler : public ICommandHandler
{
	public:
		explicit CommandHandler(IServerContext &context);
		~CommandHandler(void);

		/* ---- ICommandHandler ---- */
		void	execute(Client &client, const std::string &line);
		void	onClientDisconnect(Client &client);

	private:
		CommandHandler(const CommandHandler &other);
		CommandHandler	&operator=(const CommandHandler &other);

		/* ---- parsing ----
		 * Splits a raw line into an uppercase command name and its
		 * parameters. Handles the IRC trailing-parameter rule: a token
		 * starting with ':' consumes the rest of the line as one param. */
		void	tokenize(const std::string &line, std::string &command,
					std::vector<std::string> &params) const;

		/* ---- reply helper ---- */
		std::string	numericReply(int code, const Client &client,
						const std::string &text) const;

		/* ---- registration helpers ---- */
		bool	isValidNick(const std::string &nick) const;
		void	maybeCompleteRegistration(Client &client);

		/* ---- registration ---- */
		void	cmdPass(Client &client, const std::vector<std::string> &params);
		void	cmdNick(Client &client, const std::vector<std::string> &params);
		void	cmdUser(Client &client, const std::vector<std::string> &params);
		void	cmdCap(Client &client, const std::vector<std::string> &params);
		void	cmdPing(Client &client, const std::vector<std::string> &params);
		void	cmdQuit(Client &client, const std::vector<std::string> &params);

		/* ---- channel membership / messaging ---- */
		void	cmdJoin(Client &client, const std::vector<std::string> &params);
		void	cmdPart(Client &client, const std::vector<std::string> &params);
		void	cmdPrivmsg(Client &client, const std::vector<std::string> &params);
		void	cmdNotice(Client &client, const std::vector<std::string> &params);

		/* ---- operator commands ---- */
		void	cmdTopic(Client &client, const std::vector<std::string> &params);
		void	cmdKick(Client &client, const std::vector<std::string> &params);
		void	cmdInvite(Client &client, const std::vector<std::string> &params);
		void	cmdMode(Client &client, const std::vector<std::string> &params);

		/* ---- helpers shared by several commands ---- */
		Channel	*findChannel(const std::string &name) const;
		Channel	*getOrCreateChannel(const std::string &name);
		void	removeClientFromChannel(Client &client, Channel &channel,
					const std::string &leaveMessage);

		IServerContext				&_context;
		std::map<std::string, Channel *>	_channels;
};

#endif
