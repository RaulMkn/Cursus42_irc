/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Numerics.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ruortiz- <ruortiz-@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/12 19:57:07 by ruortiz-          #+#    #+#             */
/*   Updated: 2026/09/12 19:58:21 by ruortiz-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef NUMERICS_HPP
# define NUMERICS_HPP

enum Numeric
{
	RPL_WELCOME          = 1,    /* registro completado                    */
	RPL_YOURHOST         = 2,
	RPL_CREATED          = 3,
	RPL_MYINFO           = 4,

	RPL_NOTOPIC          = 331,  /* el canal no tiene topic                */
	RPL_TOPIC            = 332,  /* topic actual del canal                 */
	RPL_INVITING         = 341,  /* invitacion enviada con exito           */
	RPL_NAMREPLY         = 353,  /* lista de usuarios del canal            */
	RPL_ENDOFNAMES       = 366,

	ERR_NOSUCHNICK       = 401,  /* no existe ese nick/canal               */
	ERR_NOSUCHCHANNEL    = 403,
	ERR_CANNOTSENDTOCHAN = 404,
	ERR_NORECIPIENT      = 411,
	ERR_NOTEXTTOSEND     = 412,
	ERR_UNKNOWNCOMMAND   = 421,
	ERR_NONICKNAMEGIVEN  = 431,
	ERR_ERRONEUSNICKNAME = 432,
	ERR_NICKNAMEINUSE    = 433,
	ERR_USERNOTINCHANNEL = 441,
	ERR_NOTONCHANNEL     = 442,
	ERR_USERONCHANNEL    = 443,
	ERR_NOTREGISTERED    = 451,  /* comando antes de PASS/NICK/USER        */
	ERR_NEEDMOREPARAMS   = 461,
	ERR_ALREADYREGISTRED = 462,
	ERR_PASSWDMISMATCH   = 464,
	ERR_CHANNELISFULL    = 471,
	ERR_UNKNOWNMODE      = 472,
	ERR_INVITEONLYCHAN   = 473,
	ERR_BADCHANNELKEY    = 475,
	ERR_CHANOPRIVSNEEDED = 482
};

#endif
