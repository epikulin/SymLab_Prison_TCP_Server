#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtNetwork>
#include <QtCore>

#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QAbstractBarSeries>
#include <QtCharts/QPercentBarSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QLegend>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtCore/QRandomGenerator>
#include <QtCharts/QBarCategoryAxis>
#include <QtWidgets/QApplication>
#include <QtCharts/QValueAxis>
#include <chrono>
#include <QFileDialog>
#include <time.h>

#define NumPoints 512
#define NumMuxDumpPoints 33554432
uint32_t data_alarm[16];//[NumPoints+5];
uint32_t data_peak[NumPoints];
char tcp_in[NumMuxDumpPoints];
char tcp_in_prev[10000];
QLineSeries *series,*series_alarm;

int Tick,TickNext = 0;
bool isTicked = 0;
int num_packets = 0;
bool isSocket_aux_Connected = false;
long int offset = 0, bytes;
QChart *chart;
bool dump_started = 0;

std::chrono::high_resolution_clock::time_point timepoint1,timepoint2;
std::chrono::high_resolution_clock::duration duration1;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    server = new QTcpServer(this);
    server_aux = new QTcpServer(this);
    connect(server, SIGNAL(newConnection()), SLOT(newConnection()));
    connect(server_aux, SIGNAL(newConnection()), SLOT(newConnection_aux()));

    connect(ui->pushButton, SIGNAL (pressed()), this, SLOT (HandlepushButton()));
    connect(ui->pushButton_2, SIGNAL (pressed()), this, SLOT (HandlepushButton_2()));
    connect(ui->checkBox, SIGNAL(clicked(bool)),this, SLOT(checkbox(bool)));

    qDebug() << "Listening:" << server->listen(QHostAddress::Any, 7777);
    if(ui->checkBox->isChecked())
        qDebug() << "Listening:" << server_aux->listen(QHostAddress::Any, 7778);

    chart = new QChart();
    chart->setTitle("Simlab channelizer 512 channels");
    series = new QLineSeries(chart);
    for(int i = 0; i<NumPoints; i++)
        series->append(i,0);
    chart->addSeries(series);

    series_alarm = new QLineSeries(chart);
    series_alarm->setColor(Qt::red);
    for(int i = 0; i<NumPoints; i++)
        series_alarm->append(i,0);
    chart->addSeries(series_alarm);

    chart->createDefaultAxes();
    chart->axes(Qt::Horizontal).first()->setRange(0, NumPoints-1);
    chart->axes(Qt::Vertical).first()->setRange(-130, 20);

    QValueAxis *axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
    Q_ASSERT(axisY);
    axisY->setLabelFormat("%.1f  ");

    QChartView *chartView = new QChartView(chart);

    ui->gridLayout->addWidget(chartView, 0, 0);
    m_chart = chartView;

    QPalette pal = window()->palette();
    pal.setColor(QPalette::Window, QRgb(0x40434a));
    pal.setColor(QPalette::WindowText, QRgb(0xd6d6d6));
    window()->setPalette(pal);
    chartView->chart()->legend()->hide();
    chartView->chart()->setTheme(QChart::ChartThemeBlueCerulean);
    chartView->setRenderHint(QPainter::Antialiasing, true);

    myTimer2 = new QTimer(this);
    connect(myTimer2, SIGNAL(timeout()), SLOT(mytimerEvent()));
    myTimer2->setInterval(100);//one shot after 100ms
    myTimer2->setSingleShot(true);
}
void MainWindow::checkbox(bool is_checked)
{
    if(is_checked)
        server_aux->listen(QHostAddress::Any, 7778);
    else
        server_aux->close();
}
void MainWindow::mytimerEvent(void)//(QTimerEvent *event)
{
    dump_started = false; //end of dump sending
    offset += bytes;
    ui->progressBar->setValue(0);
    if(offset >= NumMuxDumpPoints)
    {
        QString fileName_I = "dump_I.dat";
        QFile file_I(fileName_I);
        QString fileName_Q = "dump_Q.dat";
        QFile file_Q(fileName_Q);
        file_I.open(QIODevice::WriteOnly | QIODevice::Text);
        file_Q.open(QIODevice::WriteOnly | QIODevice::Text);
        QString myString_out;
        for(int i = 0; i<NumMuxDumpPoints/4; i+=4)
        {
            myString_out = QString::number((int16_t)*(int *)(tcp_in+4*i) ) + "\n";
            file_I.write(myString_out.toUtf8());
            myString_out = QString::number((int16_t)*(int *)(tcp_in+4*i+2)) + "\n";
            file_Q.write(myString_out.toUtf8());
        }
    }
    offset = 0;
}

MainWindow::~MainWindow()
{
    delete ui;
    delete server;
    delete server_aux;
}

void MainWindow::newConnection()
{
   qDebug()<<"Connected";
    while (server->hasPendingConnections())
    {
        socket = server->nextPendingConnection();
        socket->setSocketOption(QAbstractSocket::LowDelayOption,1);
        connect(socket, SIGNAL(readyRead()), SLOT(readyRead()));
        connect(socket, SIGNAL(disconnected()), SLOT(disconnected()));
    }
}
void MainWindow::newConnection_aux()
{
    qDebug()<<"Connected AUX";
    isSocket_aux_Connected = true;
    while (server_aux->hasPendingConnections())
    {
        socket_aux = server_aux->nextPendingConnection();
        socket_aux->setSocketOption(QAbstractSocket::LowDelayOption,1);
        connect(socket_aux, SIGNAL(readyRead()), SLOT(readyRead_aux()));
        connect(socket_aux, SIGNAL(disconnected()), SLOT(disconnected_aux()));
    }
}

void MainWindow::disconnected()
{
      killTimer(m_timerId);
}

void MainWindow::disconnected_aux()
{
      isSocket_aux_Connected = false;
}

void MainWindow::readyRead()
{
    this->blockSignals(true);
    uint64_t timestamp = 0;
    int current_index = -1;
    bytes = socket->read(tcp_in+offset, 10000);
    qDebug() << bytes << " bytes " << offset << " offset" ;
    if(dump_started)
    {
        myTimer2->stop();//restart timer
        myTimer2->start();
        ui->progressBar->setValue(offset/NumMuxDumpPoints);
        if(offset > NumMuxDumpPoints)
        {
            dump_started = false;
            offset = 0;
        }
        else
            offset += bytes;
        this->blockSignals(false);
        return;
    }

    QByteArray readall = QByteArray(tcp_in,bytes+offset);

    while( (current_index = readall.indexOf(QByteArray("ALRM"),current_index+1)) != -1 && !dump_started)
    {
        int  slot_number = atoi(tcp_in+current_index+7);
        readall[current_index+15] = '\n';
        float start_freq = atoi(tcp_in+current_index+9)/100;
        readall[current_index+19] = '\n';
        int num_taps = atoi(tcp_in+current_index+16);

        data_alarm[0] = *(uint32_t*)(tcp_in+current_index+20);
        data_alarm[1] = *(uint32_t*)(tcp_in+current_index+20+4);
        data_alarm[2] = *(uint32_t*)(tcp_in+current_index+20+8);
        data_alarm[3] = *(uint32_t*)(tcp_in+current_index+20+12);
        data_alarm[4] = *(uint32_t*)(tcp_in+current_index+20+16);
        data_alarm[5] = *(uint32_t*)(tcp_in+current_index+20+20);
        data_alarm[6] = *(uint32_t*)(tcp_in+current_index+20+24);
        data_alarm[7] = *(uint32_t*)(tcp_in+current_index+20+28);
        data_alarm[8] = *(uint32_t*)(tcp_in+current_index+20+32);
        data_alarm[9] = *(uint32_t*)(tcp_in+current_index+20+36);
        data_alarm[10] = *(uint32_t*)(tcp_in+current_index+20+40);
        data_alarm[11] = *(uint32_t*)(tcp_in+current_index+20+44);
        data_alarm[12] = *(uint32_t*)(tcp_in+current_index+20+48);
        data_alarm[13] = *(uint32_t*)(tcp_in+current_index+20+52);
        data_alarm[14] = *(uint32_t*)(tcp_in+current_index+20+56);
        data_alarm[15] = *(uint32_t*)(tcp_in+current_index+20+60);
        timestamp = *(uint64_t*)(tcp_in+current_index+84);
        QString mystring = QString("ALARM SLOT  %1 FREQ %2 NUM_TAPS %3 CODE %4 %5 %6 %7 %8 %9 %10 %11 %12 %13 %14 %15 %16 %17 %18 %19  stamp %20").arg(slot_number).arg(start_freq).arg(num_taps)
                .arg(data_alarm[0],32,2,QLatin1Char('0')).arg(data_alarm[1],32,2,QLatin1Char('0')).arg(data_alarm[2],32,2,QLatin1Char('0')).arg(data_alarm[3],32,2,QLatin1Char('0'))
                .arg(data_alarm[4],32,2,QLatin1Char('0')).arg(data_alarm[5],32,2,QLatin1Char('0')).arg(data_alarm[6],32,2,QLatin1Char('0')).arg(data_alarm[7],32,2,QLatin1Char('0'))
                .arg(data_alarm[8],32,2,QLatin1Char('0')).arg(data_alarm[9],32,2,QLatin1Char('0')).arg(data_alarm[10],32,2,QLatin1Char('0')).arg(data_alarm[11],32,2,QLatin1Char('0'))
                .arg(data_alarm[12],32,2,QLatin1Char('0')).arg(data_alarm[13],32,2,QLatin1Char('0')).arg(data_alarm[14],32,2,QLatin1Char('0')).arg(data_alarm[15],32,2,QLatin1Char('0'))
                .arg(timestamp);
        ui->textEdit->append(mystring);
        for(int i = 0; i<4; i++)
        {
            //qDebug() << data_tcp[i];
            for(int j=0; j<32; j++)
                series_alarm->replace(i*32+j,i*32+j,40*( (data_alarm[i] & ( 1<<(j) ) ) > 0 ) );
        }
    }
    current_index = -1;
    while( (current_index = readall.indexOf("PEAK",current_index+1) ) != -1 && !dump_started)
    {
        int  slot_number = atoi(tcp_in+current_index+7);
        readall[current_index+15] = '\n';
        float start_freq = atoi(tcp_in+current_index+9)/100;
        readall[current_index+19] = '\n';
        int num_taps = atoi(tcp_in+current_index+16);

        if(current_index+20+num_taps > bytes+offset)
        {
            offset += bytes;
            qDebug()<<"offset " << offset;
        }
        else
        {
            offset = 0;
            qDebug()<<"offset " << offset;
            for(int i=0; i<num_taps; i++)
               {
                    data_peak[i] = *(uint32_t*)(tcp_in+current_index+20+i*4);
               }
            series->clear();
            series_alarm->clear();

            for(int i=0; i<num_taps; i++)
               series->append(start_freq+i*0.24 , 20*log10(data_peak[i]+0.1) -156);

            chart->axes(Qt::Horizontal).first()->setRange(start_freq, start_freq + num_taps*0.24);

            chart->setTitle("SLOT "+QString::number(slot_number).toUtf8());

        }
    }
    current_index = -1;
    while( ((current_index = readall.indexOf("ACK",current_index+1) ) != -1) && !dump_started)
    {
        char *acktext = (char*)(tcp_in+current_index);
        ui->textEdit_3->append(acktext);
    }
    current_index = -1;
    while( ((current_index = readall.indexOf("ERR",current_index+1) ) != -1) && !dump_started)
    {
        char *acktext = (char*)(tcp_in+current_index);
        ui->textEdit_3->append(acktext);
    }
    current_index = -1;
    while( ((current_index = readall.indexOf("ANTENNA_UNCONNECTED",current_index+1) ) != -1) && !dump_started)
    {
        char *antenna_text = (char*)(tcp_in+current_index);
        ui->textEdit_3->append(antenna_text);
    }
    current_index = -1;
    while( ((current_index = readall.indexOf("DUMP",current_index+1) ) != -1) && !dump_started)
    {
        qDebug() << "DUMP " << bytes << "bytes " << offset << " offset" ;
        dump_started = true;
        ui->progressBar->setValue(0);
        offset += bytes;
        if(offset > NumMuxDumpPoints)
        {
            dump_started = false;
            offset = 0;
        }
        else
        {
            myTimer2->stop();//restart timer
            myTimer2->start();
        }
    }
    this->blockSignals(false);
}
void MainWindow::readyRead_aux()
{
    this->blockSignals(true);
    int current_index = -1;
    bytes = socket_aux->read(tcp_in+offset, 10000);
    qDebug() << bytes << " bytes aux " << offset << " offset aux " ;

    QByteArray readall = QByteArray(tcp_in,bytes+offset);

    while( ((current_index = readall.indexOf("ACK",current_index+1) ) != -1) && !dump_started)
    {
        char *acktext = (char*)(tcp_in+current_index);
        ui->textEdit_3->append(acktext);
    }
    current_index = -1;
    while( ((current_index = readall.indexOf("ERR",current_index+1) ) != -1) && !dump_started)
    {
        char *acktext = (char*)(tcp_in+current_index);
        ui->textEdit_3->append(acktext);
    }
    current_index = -1;
    this->blockSignals(false);
}

void MainWindow::HandlepushButton()
{
    qDebug() << "button 1";
    QFile file;
    QString filters("Text files (*.txt);;All files (*.*)");
    QString defaultFilter("Text files (*.txt)");
    QString fileName = QFileDialog::getOpenFileName(this,"Select file with slot configuration", QDir::currentPath(),filters, &defaultFilter);
    if(fileName!="")
    {
        ui->textEdit_2->clear();
        file.setFileName(fileName);
        file.open(QIODevice::ReadOnly);
        QTextStream in(&file);
        while(!in.atEnd())
        {
          QString line = in.readLine();
          ui->textEdit_2->append(line);
        }
    }
}
void MainWindow::HandlepushButton_2()
{
    //qDebug() << "button 2";
    ui->textEdit_3->clear();
    QString data = ui->textEdit_2->toPlainText();
    QStringList strList = data.split(QRegExp("[\n]"),QString::KeepEmptyParts);

    if(isSocket_aux_Connected)
        for (int i = 0; i < strList.size(); ++i)
            socket_aux->write(strList.at(i).toUtf8()+"\r\n");
}
