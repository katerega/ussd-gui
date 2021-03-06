/*
 *
 *  Copyright (c) 2015-2016
 *  name : Francis Banyikwa
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GSM_H
#define GSM_H

#include <QStringList>
#include <QString>
#include <QByteArray>
#include <QVector>

#include <functional>
#include <memory>

#include "task.h"

class gsm
{
public:
	struct USSDMessage
	{
		QByteArray Text ;

		enum {  NoActionNeeded,
			ActionNeeded,
			Terminated,
			AnotherClient,
			NotSupported,
			Timeout,
			Unknown } Status ;
	} ;

	struct SMSText
	{
		QString phoneNumber ;
		QString date ;
		QString message ;
		bool inSIMcard ;
		bool inInbox ;
	} ;

	static const char * decodeUnicodeString( const QByteArray& ) ;
	static gsm * instance( const QStringList&,
			       std::function< void( const gsm::USSDMessage& ussd ) >&&
			       = []( const gsm::USSDMessage& ussd ){ Q_UNUSED( ussd ) } ) ;

	virtual ~gsm() ;

	virtual Task::future< QVector< gsm::SMSText > >& getSMSMessages( bool deleteSMS = false ) = 0 ;

	virtual Task::future< bool>& connect() = 0 ;
	virtual Task::future< bool>& dial( const QByteArray& ) = 0 ;
	virtual Task::future< bool>& hasData( bool waitForData = false ) = 0 ;
	virtual Task::future< bool>& disconnect() = 0 ;

	virtual bool canRead( bool waitForData = false ) = 0 ;
	virtual bool init( bool = false ) = 0 ;
	virtual bool connected() = 0 ;
	virtual bool listenForEvents( bool = true ) = 0 ;
	virtual bool cancelCurrentOperation() = 0 ;
	virtual bool canCheckSms() = 0 ;

	virtual void setlocale( const char * = nullptr ) = 0 ;

	virtual const char * lastError() = 0 ;

	virtual QString source() = 0 ;
} ;

#endif // GSM_H
