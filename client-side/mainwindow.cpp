#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setFixedSize(this->geometry().width(),this->geometry().height());
    this->m_progressDialog = new ProgressDialog();
    this->m_retrydownloads = new QTimer();

    connect(this->m_retrydownloads, SIGNAL(timeout()), this, SLOT(retryFirmwareDownload()));
    connect(ui->btnSerialRefresh, SIGNAL(clicked()), SLOT(updateSerialPorts()));
    connect(ui->cmbPlatform, SIGNAL(currentIndexChanged(int)), SLOT(platformChanged(int)));
    connect(ui->btnFlash, SIGNAL(clicked()), SLOT(startFlash()));
    connect(ui->btnAbout, SIGNAL(clicked()), SLOT(about()));

    this->updateSerialPorts();
    this->updateConfigs();

    //Path for firmware, probably needs to be changed on macosx
    this->m_firmwareDirectoryName = qApp->applicationDirPath() + "/firmwares/";
    QDir dir;
    if (!dir.exists(this->m_firmwareDirectoryName)) {
        dir.mkdir(this->m_firmwareDirectoryName);
    }

    this->m_aboutDlg = new AboutDialog();
}

void MainWindow::about()
{
    this->m_aboutDlg->show();
}

void MainWindow::updateConfigs()
{
    connect(this->m_progressDialog, SIGNAL(downloadsFinished(DownloadsList)), this, SLOT(downloadFinishedConfigs(DownloadsList)));
    this->m_progressDialog->setLabelText(tr("Updating available firmwares..."));
    this->m_progressDialog->show();
    this->m_progressDialog->startDownloads(Download(FLASHTOOL_PATH_URI));
}

void MainWindow::downloadFinishedConfigs(DownloadsList downloads)
{
    Download download = downloads[0];

    if (!download.success) {
        QMessageBox::critical(this, tr("FlashTool"), tr("Failed to download firmware informations, try again later."));

        QFile::remove(download.tmpFile);

        QApplication::exit();
        return;
    }

    disconnect(this->m_progressDialog, SIGNAL(downloadsFinished(DownloadsList)), this, SLOT(downloadFinishedConfigs(DownloadsList)));

    QString oldBoardType = this->m_settings.value("BoardType").toString();
    QString oldRCInput = this->m_settings.value("RCInput").toString();
    QString oldRCInputMapping = this->m_settings.value("RCInputMapping").toString();
    QString oldPlatform = this->m_settings.value("Platform").toString();
    QString oldGpsType = this->m_settings.value("GpsType").toString();
    QString oldGpsBaud = this->m_settings.value("GpsBaud").toString();

    int oldBoardTypeIndex = 0;
    int oldRCInputIndex = 0;
    int oldRCInputMappingIndex = 0;
    int oldPlatformIndex = 0;
    int oldGpsTypeIndex = 0;
    int oldGpsBaudIndex = 0;

    this->m_progressDialog->setLabelText(tr("Checking available firmwares..."));

    QFile *file = new QFile(download.tmpFile);
    file->open(QIODevice::ReadOnly | QIODevice::Text);

    QXmlStreamReader xml(file);

    while (!xml.atEnd()) {
        xml.readNext();

        //Settings
        if (xml.isStartElement() && (xml.name() == "settings")) {
            this->m_globalsettings.hexurl = xml.attributes().value("hexurl").toString().simplified();
        }

        //Boards
        if (xml.isStartElement() && (xml.name() == "boards")) {
            ui->cmbBoardType->clear();
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && (xml.name() == "board")) {
                    BoardType board;
                    board.name = xml.attributes().value("name").toString().simplified();
                    board.id = xml.attributes().value("id").toString().simplified();

                    if (board.id == oldBoardType) {
                        oldBoardTypeIndex = ui->cmbBoardType->count();
                    }

                    QVariant vBoard;
                    vBoard.setValue<BoardType>(board);
                    ui->cmbBoardType->addItem(board.name, vBoard);
                }
                if (xml.isEndElement() && (xml.name() == "boards")) {
                    break;
                }
            }
        }

        //RC Inputs
        if (xml.isStartElement() && (xml.name() == "rcinputs")) {
            ui->cmbRCType->clear();
            ui->cmbRCMapping->clear();
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && (xml.name() == "rcinput")) {
                    RCInput input;
                    input.name = xml.attributes().value("name").toString().simplified();
                    input.id = xml.attributes().value("id").toString().simplified();

                    if (input.id == oldRCInput) {
                        oldRCInputIndex = ui->cmbRCType->count();
                    }

                    QVariant vInput;
                    vInput.setValue<RCInput>(input);
                    ui->cmbRCType->addItem(input.name, vInput);
                }
                if (xml.isStartElement() && (xml.name() == "rcmapping")) {
                    RCInputMapping input;
                    input.name = xml.attributes().value("name").toString().simplified();
                    input.id = xml.attributes().value("id").toString().simplified();

                    if (input.id == oldRCInputMapping) {
                        oldRCInputMappingIndex = ui->cmbRCMapping->count();
                    }

                    QVariant vInput;
                    vInput.setValue<RCInputMapping>(input);
                    ui->cmbRCMapping->addItem(input.name, vInput);
                }
                if (xml.isEndElement() && (xml.name() == "rcinputs")) {
                    break;
                }
            }
        }

        //Platforms
        if (xml.isStartElement() && (xml.name() == "platforms")) {
            ui->cmbPlatform->clear();
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && (xml.name() == "platform")) {
                    Platform platform;
                    platform.name = xml.attributes().value("name").toString().simplified();
                    platform.id = xml.attributes().value("id").toString().simplified();
                    platform.image = xml.attributes().value("image").toString().simplified();
                    platform.version = xml.attributes().value("version").toString().simplified();

                    if (platform.id == oldPlatform) {
                        oldPlatformIndex = ui->cmbPlatform->count();
                    }

                    QVariant vPlatform;
                    vPlatform.setValue<Platform>(platform);
                    ui->cmbPlatform->addItem(platform.name, vPlatform);
                }
                if (xml.isEndElement() && (xml.name() == "platforms")) {
                    break;
                }
            }
        }

        //GPS
        if (xml.isStartElement() && (xml.name() == "gps")) {
            ui->cmbGpsType->clear();
            ui->cmbGpsBaud->clear();
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && (xml.name() == "gpstype")) {
                    GpsType gpstype;
                    gpstype.name = xml.attributes().value("name").toString().simplified();
                    gpstype.id = xml.attributes().value("id").toString().simplified();

                    if (gpstype.id == oldGpsType) {
                        oldGpsTypeIndex = ui->cmbGpsType->count();
                    }

                    QVariant vGpsType;
                    vGpsType.setValue<GpsType>(gpstype);
                    ui->cmbGpsType->addItem(gpstype.name, vGpsType);
                }
                if (xml.isStartElement() && (xml.name() == "gpsbaud")) {
                    GpsBaudrate gpsbaud;
                    gpsbaud.name = xml.attributes().value("name").toString().simplified();
                    gpsbaud.id = xml.attributes().value("id").toString().simplified();

                    if (gpsbaud.id == oldGpsBaud) {
                        oldGpsBaudIndex = ui->cmbGpsBaud->count();
                    }

                    QVariant vGpsBaud;
                    vGpsBaud.setValue<GpsBaudrate>(gpsbaud);
                    ui->cmbGpsBaud->addItem(gpsbaud.name, vGpsBaud);
                }
                if (xml.isEndElement() && (xml.name() == "gps")) {
                    break;
                }
            }
        }

        //Versions
        if (xml.isStartElement() && (xml.name() == "versions")) {
            this->m_versionList.clear();
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && (xml.name() == "version")) {
                    Version version;
                    version.number = xml.attributes().value("number").toString().simplified();
                    version.id = xml.attributes().value("id").toString().simplified();
                    version.platform = xml.attributes().value("platform").toString().simplified();
                    this->m_versionList<<version;
                }
                if (xml.isEndElement() && (xml.name() == "versions")) {
                    break;
                }
            }
        }
    }

    file->close();
    file->remove();

    //Now set old values if they still exists
    ui->cmbBoardType->setCurrentIndex(oldBoardTypeIndex);
    ui->cmbRCType->setCurrentIndex(oldRCInputIndex);
    ui->cmbRCMapping->setCurrentIndex(oldRCInputMappingIndex);
    ui->cmbPlatform->setCurrentIndex(oldPlatformIndex);
    ui->cmbGpsBaud->setCurrentIndex(oldGpsBaudIndex);
    ui->cmbGpsType->setCurrentIndex(oldGpsTypeIndex);
    this->platformChanged(ui->cmbPlatform->currentIndex());
    this->m_progressDialog->hide();
}

void MainWindow::updateSerialPorts()
{
    ui->cmbSerialPort->clear();
    ui->cmbSerialPort->setDisabled(false);
    foreach (QSerialPortInfo info, QSerialPortInfo::availablePorts()) {
        if (!info.portName().isEmpty()) {
            ui->cmbSerialPort->addItem(info.portName());
        }
    }

    if (ui->cmbSerialPort->count() == 0)
    {
        ui->cmbSerialPort->setDisabled(true);
        ui->cmbSerialPort->addItem(tr("- no serial port found -"));
    }
}

void MainWindow::platformChanged(int index)
{
    QString oldVersion = this->m_settings.value("Version").toString();
    int oldVersionIndex = 0;

    Platform platform = ui->cmbPlatform->itemData(index).value<Platform>();
    ui->lblImage->setStyleSheet("background: transparent url(:/images/resources/" + platform.image + ") center 0 no-repeat;");
    ui->cmbVersion->clear();
    ui->cmbVersion->setDisabled(false);
    for (int i = 0; i < this->m_versionList.count(); i++)
    {
        Version version = this->m_versionList[i];
        if (version.platform == platform.version)
        {
            if (version.id == oldVersion) {
                oldVersionIndex = ui->cmbVersion->count();
            }
            QVariant vVersion;
            vVersion.setValue<Version>(version);
            ui->cmbVersion->addItem(version.number, vVersion);
        }
    }

    if (ui->cmbVersion->count() == 0)
    {
        ui->cmbVersion->setDisabled(true);
        ui->cmbVersion->addItem(tr("- no flashable version found -"));
    } else {
        ui->cmbVersion->setCurrentIndex(oldVersionIndex);
    }
}

void MainWindow::startFlash()
{
    if (!ui->cmbSerialPort->isEnabled())
    {
        QMessageBox::critical(this, tr("FlashTool"), tr("No serial port found, please make sure you connected your board via usb."));
        return;
    }

    if (!ui->cmbVersion->isEnabled())
    {
        QMessageBox::critical(this, tr("FlashTool"), tr("No flashable version found."));
        return;
    }

    BoardType board = ui->cmbBoardType->itemData(ui->cmbBoardType->currentIndex()).value<BoardType>();
    RCInput rcinput = ui->cmbRCType->itemData(ui->cmbRCType->currentIndex()).value<RCInput>();
    RCInputMapping rcinputmapping = ui->cmbRCMapping->itemData(ui->cmbRCMapping->currentIndex()).value<RCInputMapping>();
    Platform platform = ui->cmbPlatform->itemData(ui->cmbPlatform->currentIndex()).value<Platform>();
    Version version = ui->cmbVersion->itemData(ui->cmbVersion->currentIndex()).value<Version>();
    GpsType gpstype = ui->cmbGpsType->itemData(ui->cmbGpsType->currentIndex()).value<GpsType>();
    GpsBaudrate gpsbaud = ui->cmbGpsBaud->itemData(ui->cmbGpsBaud->currentIndex()).value<GpsBaudrate>();

    connect(this->m_progressDialog, SIGNAL(downloadsFinished(DownloadsList)), this, SLOT(firmwareRequestDone(DownloadsList)));
    connect(this->m_progressDialog, SIGNAL(canceled()), this, SLOT(canceledDownloadFirmware()));
    this->m_progressDialog->setLabelText((tr("Requesting firmware %1 (%2) ...").arg(platform.name).arg(version.number)));
    this->m_progressDialog->show();

    QString request;

    request.append("<?xml version=\"1.0\"?>");
    request.append("<xml>");
    request.append("<board>" + board.id + "</board>");
    request.append("<rcinput>" + rcinput.id + "</rcinput>");
    request.append("<rcmapping>" + rcinputmapping.id + "</rcmapping>");
    request.append("<platform>" + platform.id + "</platform>");
    request.append("<version>" + version.id + "</version>");
    request.append("<gpstype>" + gpstype.id + "</gpstype>");
    request.append("<gpsbaud>" + gpsbaud.id + "</gpsbaud>");
    request.append("</xml>");

    this->m_progressDialog->startDownloads(Download(this->m_globalsettings.hexurl, request));
}

void MainWindow::firmwareRequestDone(DownloadsList downloads)
{
    Download download = downloads[0];

    disconnect(this->m_progressDialog, SIGNAL(downloadsFinished(DownloadsList)), this, SLOT(firmwareRequestDone(DownloadsList)));

    this->m_progressDialog->setLabelText(tr("Waiting for firmware..."));

    QFile *file = new QFile(download.tmpFile);
    file->open(QIODevice::ReadOnly | QIODevice::Text);

    QXmlStreamReader xml(file);

    QString firmwareFile;
    QString error;

    while (!xml.atEnd()) {
        xml.readNext();

        //Firmware
        if (xml.isStartElement() && (xml.name() == "firmware")) {
            xml.readNext();
            firmwareFile = xml.text().toString().simplified();
        }

        //Error
        if (xml.isStartElement() && (xml.name() == "error")) {
            xml.readNext();
            error = xml.text().toString().simplified();
        }
    }

    file->close();
    file->remove();

    if (!download.success) {
        disconnect(this->m_progressDialog, SIGNAL(canceled()), this, SLOT(canceledDownloadFirmware()));
        this->m_progressDialog->hide();
        QMessageBox::critical(this, tr("FlashTool"), tr("An error occured on the build server: %1").arg(error));
        return;
    }

    this->m_firmwareFileName = firmwareFile;

    if (QFile::exists(this->m_firmwareDirectoryName + this->m_firmwareFileName)) {
        disconnect(this->m_progressDialog, SIGNAL(canceled()), this, SLOT(canceledDownloadFirmware()));
        flashFirmware(this->m_firmwareDirectoryName + this->m_firmwareFileName);
    } else {
        DownloadsList firmwareDownloads;
        firmwareDownloads<<Download(this->m_globalsettings.hexurl + "/" + firmwareFile + ".gz");
        firmwareDownloads<<Download(this->m_globalsettings.hexurl + "/" + firmwareFile + ".md5");

        connect(this->m_progressDialog, SIGNAL(downloadsFinished(DownloadsList)), this, SLOT(downloadFinishedFirmware(DownloadsList)));
        this->m_progressDialog->startDownloads(firmwareDownloads);
    }
}

void MainWindow::downloadFinishedFirmware(DownloadsList downloads)
{
    Download download = downloads[0];
    Download downloadMd5 = downloads[1];

    disconnect(this->m_progressDialog, SIGNAL(downloadsFinished(DownloadsList)), this, SLOT(downloadFinishedFirmware(DownloadsList)));
    disconnect(this->m_progressDialog, SIGNAL(canceled()), this, SLOT(canceledDownloadFirmware()));

    if (!download.success) {
        QFile::remove(download.tmpFile);
        QFile::remove(downloadMd5.tmpFile);
        this->m_currentFirmwareDownloads = downloads;
        this->m_retrydownloads->start(10000);
        QString dots = ".";
        this->m_progressDialog->setLabelText(tr("Waiting for firmware") + dots.repeated(download.tries % 3));
        if (download.tries > 30) {
            QMessageBox::critical(this, tr("FlashTool"), tr("Failed to download firmware, try again later."));
        }
    } else {

        this->m_progressDialog->hide();

        //get md5 from server file
        QFile md5File(downloadMd5.tmpFile);
        md5File.open(QIODevice::ReadOnly);
        QTextStream in(&md5File);
        QString md5sumReference;
        while(!in.atEnd()) {
            QString line = in.readLine();
            md5sumReference = line.left(32);
            break;
        }
        md5File.close();
        md5File.remove();

        //calculate md5 from downloaded file
        QFile hexFile(download.tmpFile);
        hexFile.open(QIODevice::ReadOnly);

        QByteArray data = hexFile.readAll();

        //Check if file is gz compressed and decompress
        if (data.count() >= 10 || data.at(0) == 0x1f) {
            data = gzipDecompress(data);
        }

        QString md5sum = QString(QCryptographicHash::hash(data,QCryptographicHash::Md5).toHex());
        hexFile.close();

        if (md5sum != md5sumReference)
        {
            QMessageBox::critical(this, tr("FlashTool"), tr("The downloaded firmware looks corrupted, please try again."));
            return;
        }

        QString hexFilename = this->m_firmwareDirectoryName + this->m_firmwareFileName;
        hexFile.remove();
        QFile finalHexFile(hexFilename);
        finalHexFile.open(QIODevice::WriteOnly);
        finalHexFile.write(data);
        finalHexFile.close();;
        flashFirmware(hexFilename);
    }
}

void MainWindow::retryFirmwareDownload()
{
    this->m_retrydownloads->stop();
    connect(this->m_progressDialog, SIGNAL(downloadsFinished(DownloadsList)), this, SLOT(downloadFinishedFirmware(DownloadsList)));
    connect(this->m_progressDialog, SIGNAL(canceled()), this, SLOT(canceledDownloadFirmware()));
    this->m_progressDialog->startDownloads(this->m_currentFirmwareDownloads);
}

void MainWindow::flashFirmware(QString filename)
{
    if (!QFile::exists(filename)) {
        QMessageBox::critical(this, tr("FlashTool"), tr("Firmware not found."));
        return;
    }

    QString program = qApp->applicationDirPath() + "/external/avrdude.exe";
    QStringList arguments;
    arguments << "-C" + qApp->applicationDirPath() + "/external/avrdude.conf";
    arguments << "-patmega2560";
    arguments << "-cwiring";
    arguments << "-P" + ui->cmbSerialPort->currentText();
    arguments << "-b115200";
    arguments << "-D";
    arguments << "-Uflash:w:" + filename + ":i";

    this->m_avrdudeOutput = ">" + program + " " + arguments.join(" ");
    this->m_process = new QProcess;

    connect(this->m_progressDialog, SIGNAL(canceled()), this, SLOT(canceledFirmwareUpload()));
    connect(this->m_process,SIGNAL(readyReadStandardOutput()),this, SLOT(avrdudeReadStandardOutput()));
    connect(this->m_process,SIGNAL(readyReadStandardError()),this, SLOT(avrdudeReadStandardError()));
    connect(this->m_process,SIGNAL(finished(int)),this, SLOT(avrdudeFinished(int)));
    connect(this->m_process,SIGNAL(error(QProcess::ProcessError)),this, SLOT(avrdudeError(QProcess::ProcessError)));

    this->m_progressDialog->show();
    this->m_progressDialog->setLabelText(tr("Starting flashing process..."));
    this->m_process->start(program, arguments);
}


QByteArray MainWindow::gzipDecompress(QByteArray compressData)
{
    if (compressData.size() <= 4) {
        qWarning("gUncompress: Input data is truncated");
        return QByteArray();
    }

    QByteArray result;

    int ret;
    z_stream strm;
    static const int CHUNK_SIZE = 1024;
    char out[CHUNK_SIZE];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = compressData.size();
    strm.next_in = (Bytef*)(compressData.data());

    ret = inflateInit2(&strm, 15 +  32); // gzip decoding
    if (ret != Z_OK)
        return QByteArray();

    // run inflate()
    do {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = (Bytef*)(out);

        ret = inflate(&strm, Z_NO_FLUSH);
        Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;     // and fall through
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            (void)inflateEnd(&strm);
            return QByteArray();
        }

        result.append(out, CHUNK_SIZE - strm.avail_out);
    } while (strm.avail_out == 0);

    // clean up and return
    inflateEnd(&strm);
    return result;
}

void MainWindow::canceledFirmwareUpload()
{
    disconnect(this->m_progressDialog, SIGNAL(canceled()), this, SLOT(canceledFirmwareUpload()));
    this->m_process->kill();
    QMessageBox::critical(this, tr("FlashTool"), tr("You canceled the firmware upload!"));
}

void MainWindow::parseAvrdudeOutput()
{
    QString output = this->m_avrdudeOutput;
    qDebug()<<output;
    this->m_progressDialog->setMaximum(50);

    if (output.contains("AVR device initialized and ready to accept instructions")) {

        if (!output.contains("bytes of flash written")) {
            QString writing = output.mid(output.lastIndexOf("Writing |"), 61);
            this->m_progressDialog->setValue(writing.count("#"));
            this->m_progressDialog->setLabelText(tr("Writing firmware please wait..."));
        } else {
            QString reading = output.mid(output.lastIndexOf("Reading |"), 61);
            this->m_progressDialog->setValue(reading.count("#"));
            this->m_progressDialog->setLabelText(tr("Verifying firmware please wait..."));
        }
    }
}

void MainWindow::avrdudeError(QProcess::ProcessError error)
{
    disconnect(this->m_progressDialog, SIGNAL(canceled()), this, SLOT(canceledFirmwareUpload()));
    this->m_progressDialog->hide();

    QString errorMsg;

    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = tr("Failed to start avrdude. (executable missing?)");
        break;
    case QProcess::Crashed:
        errorMsg = tr("avrdude crashed somehow.");
        break;
    case QProcess::Timedout:
    case QProcess::ReadError:
    case QProcess::WriteError:
    case QProcess::UnknownError:
    default:
        errorMsg = tr("Some internal error occured. Errorcode: %1").arg(error);
        break;
    }

    QMessageBox::critical(this, tr("FlashTool"), tr("An error occured with avrdude: \n\n%1").arg(errorMsg));
}

void MainWindow::avrdudeFinished(int exitCode)
{
    disconnect(this->m_progressDialog, SIGNAL(canceled()), this, SLOT(canceledFirmwareUpload()));

    this->m_progressDialog->hide();
    if (exitCode == 0) {
        QMessageBox::information(this, tr("FlashTool"), tr("Firmware flashed successfully!"));
    } else {
        QString errorFilename = qApp->applicationDirPath() + "/error.txt";
        QFile errorFile(errorFilename);
        errorFile.open(QIODevice::ReadWrite);
        errorFile.write(this->m_avrdudeOutput.toLatin1());
        errorFile.close();
        QMessageBox::critical(this, tr("FlashTool"), tr("Flashing failed, please consulte the error.txt file located here: %1").arg(errorFilename));
    }
}

void MainWindow::avrdudeReadStandardOutput()
{
    this->m_avrdudeOutput.append(this->m_process->readAllStandardOutput());
    this->parseAvrdudeOutput();
}

void MainWindow::avrdudeReadStandardError()
{
    this->m_avrdudeOutput.append(this->m_process->readAllStandardError());
    this->parseAvrdudeOutput();
}

void MainWindow::canceledDownloadFirmware()
{
    this->m_retrydownloads->stop();
    disconnect(this->m_progressDialog, SIGNAL(canceled()), this, SLOT(canceledDownloadFirmware()));
    disconnect(this->m_progressDialog, SIGNAL(downloadsFinished(DownloadsList)), this, SLOT(downloadFinishedFirmware(DownloadsList)));
    QMessageBox::critical(this, tr("FlashTool"), tr("You either canceled the firmware download or the download timed out."));
}

MainWindow::~MainWindow()
{
    BoardType board = ui->cmbBoardType->itemData(ui->cmbBoardType->currentIndex()).value<BoardType>();
    RCInput rcinput = ui->cmbRCType->itemData(ui->cmbRCType->currentIndex()).value<RCInput>();
    RCInputMapping rcinputmapping = ui->cmbRCMapping->itemData(ui->cmbRCMapping->currentIndex()).value<RCInputMapping>();
    Platform platform = ui->cmbPlatform->itemData(ui->cmbPlatform->currentIndex()).value<Platform>();
    Version version = ui->cmbVersion->itemData(ui->cmbVersion->currentIndex()).value<Version>();
    GpsType gpstype = ui->cmbGpsType->itemData(ui->cmbGpsType->currentIndex()).value<GpsType>();
    GpsBaudrate gpsbaud = ui->cmbGpsBaud->itemData(ui->cmbGpsBaud->currentIndex()).value<GpsBaudrate>();

    this->m_settings.setValue("BoardType", board.id);
    this->m_settings.setValue("RCInput", rcinput.id);
    this->m_settings.setValue("RCInputMapping", rcinputmapping.id);
    this->m_settings.setValue("Platform", platform.id);
    this->m_settings.setValue("Version", version.id);
    this->m_settings.setValue("GpsType", gpstype.id);
    this->m_settings.setValue("GpsBaud", gpsbaud.id);

    delete ui;
}
