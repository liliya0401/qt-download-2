#include "QuickDownload/src/quickdownload.h"
#include <QtDebug>


#if defined(QUICKDOWNLOAD_AUTO_REGISTER)
#include "register_quickdownload.h"
#endif

QuickDownloadMaster *QuickDownloadMaster::self = 0;

QuickDownloadMaster::QuickDownloadMaster(QObject *parent):
    QObject(parent)
{
    _ready = false;
    qDebug() << "DEBUGTXT QuickDownloadMaster";
    Q_ASSERT_X(!self, "QuickDownloadMaster", "there should be only one instance of this object");
    QuickDownloadMaster::self = this;

    _networkAccessManager = 0;
    _ownNetworkAccessManager = false;

    _ready = true;
    emit readyChanged();
}

QuickDownloadMaster::~QuickDownloadMaster()
{
    qDebug() << "DEBUGTXT ~QuickDownloadMaster";
    if(_ownNetworkAccessManager) {
        delete _networkAccessManager;
        _networkAccessManager = 0;
    }
}

QuickDownloadMaster *QuickDownloadMaster::instance()
{
    qDebug() << "DEBUGTXT instance";
   if(self == 0)
       self = new QuickDownloadMaster(0);
   return self;
}

bool QuickDownloadMaster::ready()
{
    qDebug() << "DEBUGTXT ready";
    return _ready;
}

bool QuickDownloadMaster::checkInstance(const char *function)
{
    bool b = (QuickDownloadMaster::self != 0);
    qDebug() << "DEBUGTXT checkInstance";
    if (!b)
        qWarning("QuickDownloadMaster::%s: Please instantiate the QuickDownloadMaster object first", function);
    return b;
}


QNetworkAccessManager *QuickDownloadMaster::networkAccessManager()
{
    qDebug() << "DEBUGTXT networkAccessManager";
    if(_networkAccessManager == 0) {
        _networkAccessManager = new QNetworkAccessManager(self);
        _ownNetworkAccessManager = true;
    }
    return _networkAccessManager;
}

void QuickDownloadMaster::setNetworkAccessManager(QNetworkAccessManager *networkAccessManager)
{
    qDebug() << "DEBUGTXT setNetworkAccessManager";
    if(_ownNetworkAccessManager && _networkAccessManager) {
        delete _networkAccessManager;
        _networkAccessManager = 0;
        _ownNetworkAccessManager = false;
    }
    _networkAccessManager = networkAccessManager;
}

/*
 * QuickDownload
 */
QuickDownload::QuickDownload(QObject *parent):
    QObject(parent)
{
    qDebug() << "DEBUGTXT QuickDownload";
    _networkReply = nullptr;
    _saveFile = nullptr;
    _componentComplete = false;
    _running = false;
    _overwrite = false;
    _progress = 0.0;
    _followRedirects = false;
}

QuickDownload::~QuickDownload()
{
    qDebug() << "DEBUGTXT ~QuickDownload";
    if(_networkReply) {
        if(_networkReply->isRunning())
            _networkReply->abort();
        shutdownNetworkReply();
    }

    if(_saveFile) {
        _saveFile->cancelWriting();
        shutdownSaveFile();
    }
}

QUrl QuickDownload::url() const
{
    qDebug() << "DEBUGTXT url";
    return _url;
}

void QuickDownload::setUrl(const QUrl &url)
{
    qDebug() << "DEBUGTXT setUrl";
    if(_url != url) {
        _url = url;
        emit urlChanged();
    }
}

bool QuickDownload::running() const
{
    qDebug() << "DEBUGTXT running";
    return _running;
}

void QuickDownload::setRunning(bool running)
{
    qDebug() << "DEBUGTXT setRunning";
    if(_running != running) {
        _running = running;
        if(!_running) {

            if(_networkReply) {
                if(_networkReply->isRunning())
                    _networkReply->abort();
                shutdownNetworkReply();
            }

            if(_saveFile) {
                _saveFile->cancelWriting();
                shutdownSaveFile();
            }
        } else
            start();

        emit runningChanged();
    }

}

qreal QuickDownload::progress() const
{
    qDebug() << "DEBUGTXT progress";
    return _progress;
}

QUrl QuickDownload::destination() const
{
    qDebug() << "DEBUGTXT destination";
    return _destination;
}

void QuickDownload::setDestination(const QUrl &destination)
{
    qDebug() << "DEBUGTXT setDestination";
    if(_destination != destination) {
        _destination = destination;
        if(_saveFile && !_running) {
            QString newDestination = _destination.toDisplayString(QUrl::PreferLocalFile);
            if(_saveFile->fileName() != newDestination)
                _saveFile->setFileName(newDestination);
        }
        emit destinationChanged();
    }
}

bool QuickDownload::followRedirects() const
{
    qDebug() << "DEBUGTXT followRedirects";
    return _followRedirects;
}

void QuickDownload::setFollowRedirects(bool followRedirects)
{
    qDebug() << "DEBUGTXT setFollowRedirects";
    if(_followRedirects != followRedirects) {
        _followRedirects = followRedirects;
        emit followRedirectsChanged();
    }
}

void QuickDownload::componentComplete()
{
    qDebug() << "DEBUGTXT componentComplete";
    _componentComplete = true;
    if(_running)
        start();
}

void QuickDownload::start(QUrl url)
{
    qDebug() << "DEBUGTXT start";
    if(!_componentComplete)
        return;

    if(url.isEmpty()) {
        emit error(Error::ErrorUrl,"Url is empty");
        return;
    }

    if(_destination.isEmpty()) {
        emit error(Error::ErrorDestination,"Destination is empty");
        return;
    }

    setUrl(url);

    QString destination = _destination.toDisplayString(QUrl::PreferLocalFile);

    if (QFile::exists(destination)) {
        if(!_overwrite) {
            emit error(Error::ErrorDestination,"Overwriting not allowed for destination file \""+destination+"\"");
            return;
        }
    }

    // Cancel and delete any previous open _saveFile disregarding it's state
    if(_saveFile)
        _saveFile->cancelWriting();
    shutdownSaveFile();
    _saveFile = new QSaveFile(destination);
    if (!_saveFile->open(QIODevice::WriteOnly)) {
        emit error(Error::ErrorDestination,_saveFile->errorString());
        shutdownSaveFile();
        return;
    }

    // Shutdown any previous used replies
    shutdownNetworkReply();
    _networkReply = qQuickDownloadMaster->networkAccessManager()->get(QNetworkRequest(_url));

    connect(_networkReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(_networkReply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(onDownloadProgress(qint64,qint64)));
    connect(_networkReply, SIGNAL(finished()), this, SLOT(onFinished()));

    setProgress(0.0);
    setRunning(true);
    emit started();
}

void QuickDownload::start()
{
    qDebug() << "DEBUGTXT start";
    start(_url);
}

void QuickDownload::stop()
{
    qDebug() << "DEBUGTXT stop";
    setRunning(false);
}

void QuickDownload::onReadyRead()
{
    qDebug() << "DEBUGTXT onReadyRead";
    if (_saveFile)
        _saveFile->write(_networkReply->readAll());
}

void QuickDownload::onFinished()
{
    qDebug() << "DEBUGTXT componentComplete";
    if (!_running) {
        if(_saveFile)
            _saveFile->cancelWriting();
    }
    if(!_networkReply) {
        emit error(Error::ErrorNetwork,"Network reply was deleted");
        if(_saveFile)
            _saveFile->cancelWriting();
        shutdownSaveFile();
        return;
    }

    // get redirection url
    QVariant redirectionTarget = _networkReply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (_networkReply->error()) {
        _saveFile->cancelWriting();
        emit error(Error::ErrorNetwork,_networkReply->errorString());
    } else if (!redirectionTarget.isNull()) {
        QUrl newUrl = _url.resolved(redirectionTarget.toUrl());

        emit redirected(newUrl);

        if(_followRedirects) {

            start(newUrl);
            return;
        } else {
            emit error(Error::ErrorNetwork,"Re-directs not allowed");
        }
    } else {
        if(_saveFile->commit()) {
            shutdownSaveFile();
            setProgress(1.0);
            setRunning(false);
            emit finished();
        } else {
            if(_saveFile)
                _saveFile->cancelWriting();
            emit error(Error::ErrorDestination,"Error while writing \""+_destination.toDisplayString(QUrl::PreferLocalFile)+"\"");
        }
    }

    shutdownNetworkReply();
    shutdownSaveFile();
}

void QuickDownload::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    qDebug() << "DEBUGTXT onDownloadProgress";
    if(!_running)
        return;
    emit update((bytesReceived / 1000), (bytesTotal / 1000));
    setProgress(((qreal)bytesReceived / bytesTotal));
}

void QuickDownload::setProgress(qreal progress)
{
    qDebug() << "DEBUGTXT setProgress";
    if(_progress != progress) {
        _progress = progress;
        emit progressChanged();
    }
}

bool QuickDownload::overwrite() const
{
    qDebug() << "DEBUGTXT overwrite";
    return _overwrite;
}

void QuickDownload::setOverwrite(bool allowOverwrite)
{
    qDebug() << "DEBUGTXT setOverwrite";
    if(_overwrite != allowOverwrite) {
        _overwrite = allowOverwrite;
        emit overwriteChanged();
    }
}

void QuickDownload::shutdownNetworkReply()
{
    qDebug() << "DEBUGTXT shutdownNetworkReply";
    if(_networkReply) {
        disconnect(_networkReply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
        disconnect(_networkReply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(onDownloadProgress(qint64,qint64)));
        disconnect(_networkReply, SIGNAL(finished()), this, SLOT(onFinished()));

        _networkReply->deleteLater();
        _networkReply = 0;
    }
}

void QuickDownload::shutdownSaveFile()
{
    qDebug() << "DEBUGTXT shutdownSaveFile";
    if(_saveFile) {
        _saveFile->commit();
        delete _saveFile;
        _saveFile = 0;
    }
}
