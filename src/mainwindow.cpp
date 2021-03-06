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

#include "dimensions.h"

#include "mainwindow.h"

#if DYNAMIC_DIMENSIONS

#include "ui_mainwindow.h"

#else

#include "ui_mainwindow_1.h"

#endif

#include <QCoreApplication>
#include <QDir>
#include <QStringList>

#include <QTranslator>

#include <QDebug>

#include "language_path.h"

#include "../3rd_party/qgsmcodec.h"

#include "favorites.h"
#include "utility.h"

static QStringList _source( QSettings& settings )
{
	auto _option = [ & ]( const QString& key,QString value ){

		if( settings.contains( key ) ){

			return settings.value( key ).toString() ;
		}else{
			settings.setValue( key,value ) ;
			return value ;
		}
	} ;

	return { _option( "source","libgammu" ),_option( "device","/dev/ttyACM0" ),_option( "initCommand","" ) } ;
}

MainWindow::MainWindow( bool log ) :
	m_ui( new Ui::MainWindow ),
	m_settings( "ussd-gui","ussd-gui" ),
	m_gsm( gsm::instance( _source( m_settings ),[ this ]( const gsm::USSDMessage& ussd ){ this->processResponce( ussd ) ; } ) )
{
	this->setLocalLanguage() ;

	m_ui->setupUi( this ) ;

	m_timer.setTextEdit( m_ui->textEditResult ) ;

	m_ui->pbConnect->setFocus() ;

	m_ui->pbConnect->setDefault( true ) ;

	m_ui->pbSMS->setEnabled( false ) ;

	m_ui->groupBox->setTitle( QString() ) ;

	QCoreApplication::setApplicationName( "ussd-gui" ) ;

	this->setWindowIcon( QIcon( ":/ussd-gui" ) ) ;

#if DYNAMIC_DIMENSIONS

	auto f = utility::getWindowDimensions( m_settings,"main" ) ;

	auto r = f.begin() ;

	this->window()->setGeometry( *( r + 0 ),*( r + 1 ),*( r + 2 ),*( r + 3 ) ) ;
#else
	this->setFixedSize( this->size() ) ;
#endif
	connect( m_ui->pbConnect,SIGNAL( pressed() ),this,SLOT( pbConnect() ) ) ;
	connect( m_ui->pbCancel,SIGNAL( pressed() ),this,SLOT( pbQuit() ) ) ;
	connect( m_ui->pbSend,SIGNAL( pressed() ),this,SLOT( pbSend() ) ) ;
	connect( m_ui->pbConvert,SIGNAL( pressed() ),this,SLOT( pbConvert() ) ) ;
	connect( m_ui->pbSMS,SIGNAL( pressed() ),this,SLOT( pbSMS() ) ) ;

	connect( &m_menu,SIGNAL( triggered( QAction * ) ),this,SLOT( ussdCodeInfo( QAction * ) ) ) ;

	connect( &m_menu,SIGNAL( aboutToShow() ),this,SLOT( aboutToShow() ) ) ;

	m_ui->pbHistory->setMenu( &m_menu ) ;

	m_menu.setTitle( tr( "USSD Codes" ) ) ;

	m_ui->pbConvert->setEnabled( false ) ;

	this->disableSending() ;

	auto e = QDir::homePath() + "/.config/ussd-gui" ;

	QDir d ;
	d.mkpath( e ) ;

	m_settings.setPath( QSettings::IniFormat,QSettings::UserScope,e ) ;

	m_timeout = this->timeOutInterval() ;

	m_autowaitInterval = this->autowaitInterval() ;

	m_ui->lineEditUSSD_code->setText( this->topHistory() ) ;

	if( !m_gsm->init( log ) ){

		m_ui->textEditResult->setText( QObject::tr( "Status: ERROR 1: " ) + m_gsm->lastError() ) ;

		this->disableSending() ;
	}
}

QString MainWindow::topHistory()
{
	auto l = favorites::readFavorites( m_settings ) ;

	if( l.isEmpty() ){

		return QString() ;
	}else{
		return utility::split( l.first()," - " ).first() ;
	}
}

void MainWindow::aboutToShow()
{
	m_menu.clear() ;

	m_menu.addAction( tr( "Edit Favorite USSD Codes" ) ) ;
	m_menu.addSeparator() ;

	auto k = favorites::readFavorites( m_settings ) ;

	if( k.isEmpty() ){

		m_menu.addAction( "Empty" )->setEnabled( false ) ;
	}else{
		for( const auto& it : k ){

			m_menu.addAction( it ) ;
		}
	}
}

void MainWindow::ussdCodeInfo( QAction * ac )
{
	auto e = ac->text() ;

	e.remove( "&" ) ;

	if( e == tr( "Edit Favorite USSD Codes" ) ){

		favorites::instance( this,m_settings ) ;
	}else{
		m_ui->lineEditUSSD_code->setText( utility::split( e," - " ).first() ) ;
	}
}

MainWindow::~MainWindow()
{
	const auto& r = this->window()->geometry() ;

	utility::setWindowDimensions( m_settings,"main",{ r.x(),r.y(),r.width(),r.height() } ) ;
}

void MainWindow::pbSMS()
{
	m_ui->textEditResult->setText( QString() ) ;

	m_ui->groupBox->setTitle( QString() ) ;

	this->disableSending() ;

	m_ui->pbSMS->setEnabled( false ) ;
	m_ui->pbConnect->setEnabled( false ) ;
	m_ui->pbCancel->setEnabled( false ) ;
	m_ui->pbConvert->setEnabled( false ) ;

	m_timer.start( tr( "Status: Retrieving Text Messages" ) ) ;

	this->wait() ;

	auto m = m_gsm->getSMSMessages().await() ;

	m_timer.stop() ;

	auto j = m.size() ;

	if( j > 0 ){

		m_ui->groupBox->setTitle( tr( "SMS messages." ) ) ;

		m_ui->textEditResult->setText( utility::arrangeSMSInAscendingOrder( utility::condenseSMS( m ) ) ) ;
	}else{
		QString e = m_gsm->lastError() ;

		if( e == "No error." || e.isEmpty() ){

			m_ui->textEditResult->setText( tr( "Status: No Text Messages Were Found." ) ) ;
		}else{
			m_ui->textEditResult->setText( tr( "Status: ERROR 7: " ) + e ) ;
		}
	}

	this->enableSending() ;

	m_ui->pbSMS->setEnabled( true ) ;
	m_ui->pbConnect->setEnabled( true ) ;
	m_ui->pbCancel->setEnabled( true ) ;
	m_ui->pbConvert->setEnabled( true ) ;
}

void MainWindow::pbConnect()
{
	m_ui->pbConnect->setEnabled( false ) ;

	this->disableSending() ;

	m_ui->pbConvert->setEnabled( false ) ;

	if( m_gsm->connected() ){

		if( m_gsm->disconnect().await() ){

			m_ui->pbSMS->setEnabled( false ) ;

			m_ui->pbConnect->setText( tr( "&Connect" ) ) ;

			m_ui->textEditResult->setText( tr( "Status: Disconnected." ) ) ;

			m_ui->lineEditUSSD_code->setText( this->topHistory() ) ;

			m_ui->pbCancel->setEnabled( false ) ;

			this->wait( 2 ) ;

			m_ui->pbCancel->setEnabled( true ) ;
		}else{
			m_ui->textEditResult->setText( tr( "Status: ERROR 6: " ) + m_gsm->lastError() ) ;
		}
	}else{
		if( this->Connect() ){

			if( m_ui->lineEditUSSD_code->text().isEmpty() ){

				m_ui->lineEditUSSD_code->setText( this->topHistory() ) ;
			}
		}
	}

	m_ui->pbConnect->setEnabled( true ) ;
}

QString MainWindow::getSetting( const QString& opt )
{
	if( m_settings.contains( opt ) ){

		return m_settings.value( opt ).toString() ;
	}else{
		return QString() ;
	}
}

bool MainWindow::getBoolSetting( const QString& opt )
{
	if( m_settings.contains( opt ) ){

		return m_settings.value( opt ).toBool() ;
	}else{
		return false ;
	}
}

void MainWindow::setSetting( const QString& key, const QString& value )
{
	m_settings.setValue( key,value ) ;
}

void MainWindow::setSetting(const QString& key,bool value )
{
	m_settings.setValue( key,value ) ;
}

int MainWindow::timeOutInterval()
{
	QString timeOut = "timeout" ;

	if( m_settings.contains( timeOut ) ){

		return m_settings.value( timeOut ).toInt() ;
	}else{
		this->setSetting( timeOut,QString( "30" ) ) ;
		return 30 ;
	}
}

int MainWindow::autowaitInterval()
{
	QString waitInterval = "autowaitInterval" ;

	if( m_settings.contains( waitInterval ) ){

		return m_settings.value( waitInterval ).toInt() ;
	}else{
		m_settings.setValue( waitInterval,QString( "2" ) ) ;

		return 2 ;
	}
}

void MainWindow::setHistoryItem( QAction * ac )
{
	auto e = ac->text() ;

	e.remove( "&" ) ;

	if( e != tr( "Empty History." ) ){

		m_ui->lineEditUSSD_code->setText( e ) ;
	}
}

bool MainWindow::Connect()
{
	m_timer.start( tr( "Status: Connecting" ) ) ;

	this->disableSending() ;

	m_ui->pbCancel->setEnabled( false ) ;

	auto connected = m_gsm->connect().await() ;

	m_timer.stop() ;

	m_ui->pbCancel->setEnabled( true ) ;

	if( connected ){

		m_ui->pbSMS->setEnabled( m_gsm->canCheckSms() ) ;

		this->enableSending() ;

		m_ui->pbConnect->setEnabled( true ) ;

		m_ui->pbConnect->setText( tr( "&Disconnect" ) ) ;

		m_ui->textEditResult->setText( tr( "Status: Connected." ) ) ;

		this->wait() ;
	}else{
		m_ui->pbConnect->setText( tr( "&Connect" ) ) ;

		m_ui->textEditResult->setText( tr( "Status: ERROR 2: " ) + m_gsm->lastError() ) ;

		this->disableSending() ;
	}

	return connected ;
}

void MainWindow::send( const QString& code )
{
	this->disableSending() ;

	QByteArray ussd ;

	if( code.isEmpty() ){

		ussd = m_ui->lineEditUSSD_code->text().toLatin1() ;

		if( !ussd.isEmpty() ){

			m_autoSend = utility::split( ussd,' ' ) ;

			ussd = m_autoSend.first().toLatin1() ;

			m_autoSend.removeFirst() ;
		}
	}else{
		ussd = code.toLatin1() ;

		m_ui->lineEditUSSD_code->setText( code ) ;
	}

	if( ussd.isEmpty() ){

		m_ui->textEditResult->setText( tr( "Status: ERROR 6: ussd code required." ) ) ;

		return this->enableSending() ;
	}

	//m_ui->pbConnect->setEnabled( false ) ;

	m_ui->pbCancel->setEnabled( false ) ;

	m_ui->textEditResult->setText( tr( "Status: Sending A Request." ) ) ;

	this->wait() ;

	m_waiting = true ;

	if( m_gsm->dial( ussd ).await() ){

		auto e = tr( "Status: Waiting For A Reply " ) ;

		auto r = 0 ;

		auto has_no_data = true ;

		while( true ){

			if( r == m_timeout ){

				auto e = QString::number( m_timeout ) ;

				m_ui->textEditResult->setText( tr( "Status: ERROR 3: No Response Within %1 Seconds." ).arg( e ) ) ;

				m_gsm->cancelCurrentOperation() ;

				this->enableSending() ;

				break ;
			}else{
				r++ ;

				if( has_no_data ){

					has_no_data = !m_gsm->canRead() ;
				}

				if( m_waiting ){

					m_ui->textEditResult->setText( e ) ;

					e += ".... " ;

					this->wait() ;
				}else{
					break ;
				}
			}
		}
	}else{
		m_ui->textEditResult->setText( tr( "Status: ERROR 4: " ) + m_gsm->lastError() ) ;

		this->enableSending() ;
	}

	//m_ui->pbConnect->setEnabled( true ) ;

	m_ui->pbCancel->setEnabled( true ) ;
}

void MainWindow::wait( int interval )
{
	utility::wait( interval ) ;
}

void MainWindow::pbSend()
{
	m_ui->groupBox->setTitle( QString() ) ;

	m_ui->pbConvert->setEnabled( false ) ;

	if( m_gsm->connected() ){

		this->send() ;
	}else{
		if( this->Connect() ){

			this->send() ;
		}
	}
}

void MainWindow::pbQuit()
{
	if( m_ui->pbCancel->isEnabled() ){

		QCoreApplication::quit() ;
	}
}

void MainWindow::disableSending()
{
	m_ui->pbSend->setEnabled( false ) ;
	m_ui->lineEditUSSD_code->setEnabled( false ) ;
	m_ui->labelInput->setEnabled( false ) ;
}

void MainWindow::enableSending()
{
	m_ui->pbSend->setEnabled( true ) ;
	m_ui->lineEditUSSD_code->setEnabled( true ) ;
	m_ui->labelInput->setEnabled( true ) ;
}

void MainWindow::updateTitle()
{
	m_ui->groupBox->setTitle( tr( "USSD Server Response." ) ) ;

	this->enableSending() ;
}

void MainWindow::invokeMethod( const char * e )
{
	QMetaObject::invokeMethod( this,e,Qt::QueuedConnection ) ;
}

void MainWindow::invokeMethod( const char * e, const QString& f )
{
	QMetaObject::invokeMethod( this,e,Qt::QueuedConnection,Q_ARG( QString,f ) ) ;
}

void MainWindow::processResponce( const gsm::USSDMessage& ussd )
{
	m_ussd = ussd ;

	this->invokeMethod( "updateTitle" ) ;

	m_waiting = false ;

	using _gsm = gsm::USSDMessage ;

	if( m_ussd.Status == _gsm::ActionNeeded || m_ussd.Status == _gsm::NoActionNeeded ){

		if( m_ussd.Status == _gsm::ActionNeeded ){

			this->invokeMethod( "serverResponse",QString() ) ;
		}

		this->invokeMethod( "enableConvert" ) ;

		this->invokeMethod( "displayResult" ) ;
	}else{
		auto _error = [ this ]( const _gsm& ussd ){

			switch( ussd.Status ){

			case _gsm::NoActionNeeded:

				return tr( "Status: No Action Needed." ) ;

			case _gsm::ActionNeeded:

				return tr( "Status: Action Needed." ) ;

			case _gsm::Terminated:

				return tr( "Status: ERROR 5: Connection Was Terminated And The Reason Given Is: \"%1\"" ).arg( m_ussd.Text.data() ) ;

			case _gsm::AnotherClient:

				return tr( "Status: ERROR 5: Another Client Replied." ) ;

			case _gsm::NotSupported:

				return tr( "Status: ERROR 5: USSD Code Is Not Supported." ) ;

			case _gsm::Timeout:

				return tr( "Status: ERROR 5: Connection Timeout." ) ;

			case _gsm::Unknown:

				return tr( "Status: ERROR 5: Unknown Error Has Occured." ) ;

			default:
				return tr( "Status: ERROR 5: Unknown Error Has Occured." ) ;
			}
		} ;

		this->invokeMethod( "serverResponse",_error( m_ussd ) ) ;
	}
}

void MainWindow::enableConvert()
{
	m_ui->pbConvert->setEnabled( true ) ;
}

void MainWindow::serverResponse( QString e )
{
	if( e.isEmpty() ){

		m_ui->lineEditUSSD_code->clear() ;
		m_ui->lineEditUSSD_code->setFocus() ;
	}

	m_ui->textEditResult->setText( e ) ;
}

static QString _decodeOption()
{
	return "decodeType" ;
}

void MainWindow::pbConvert()
{
	QString opt = _decodeOption() ;

	if( m_settings.contains( opt ) ){

		auto e = m_settings.value( opt ).toInt() ;

		if( e == 2 ){

			e = 0 ;
		}else{
			e++ ;
		}

		m_settings.setValue( opt,QString::number( e ) ) ;
	}else{
		QString e = "0" ;
		m_settings.setValue( opt,e ) ;
	}

	this->decodeText() ;
}

int MainWindow::decodeType()
{
	QString opt = _decodeOption() ;

	if( m_settings.contains( opt ) ){

		return m_settings.value( opt ).toInt() ;
	}else{
		QString e = "0" ;
		m_settings.setValue( opt,e ) ;
		return 0 ;
	}
}

void MainWindow::decodeText()
{
	auto _decode = [ this ](){

		return gsm::decodeUnicodeString( m_ussd.Text ) ;
	} ;

	switch( this->decodeType() ){

		case 0 : m_ui->textEditResult->setText( m_ussd.Text ) ;
			 break ;
		case 1 : m_ui->textEditResult->setText( _decode() ) ;
			 break ;
		case 2 : m_ui->textEditResult->setText( _decode() ) ;
	}
}

void MainWindow::displayResult()
{
	this->decodeText() ;

	if( m_autoSend.size() > 0 ){

		this->disableSending() ;

		auto first = m_autoSend.first() ;

		m_autoSend.removeFirst() ;

		this->wait( m_autowaitInterval ) ;

		this->send( first ) ;
	}
}

void MainWindow::closeEvent( QCloseEvent * e )
{
	e->ignore() ;
	this->pbQuit() ;
}

void MainWindow::setLocalLanguage()
{
	auto lang = this->getSetting( "language" ) ;

	if( !lang.isEmpty() ){

		auto r = lang.toLatin1() ;

		if( r == "english_US" ){
			/*
			 * english_US language,its the default and hence dont load anything
			 */
		}else{
			auto translator = new QTranslator( this ) ;

			translator->load( r.constData(),LANGUAGE_FILE_PATH ) ;
			QCoreApplication::installTranslator( translator ) ;
		}
	}
}
