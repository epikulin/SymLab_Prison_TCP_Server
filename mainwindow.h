#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QtNetwork>
#include <QtWidgets/QWidget>
#include <QtCharts/QChartGlobal>

QT_CHARTS_BEGIN_NAMESPACE
class QChartView;
class QChart;
QT_CHARTS_END_NAMESPACE

QT_CHARTS_USE_NAMESPACE

typedef QPair<QPointF, QString> Data;
typedef QList<Data> DataList;
typedef QList<DataList> DataTable;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QChartView *chartView;
    QTimer *myTimer2;

signals:
    void dataReceived(QByteArray);

protected:


private slots:
   void newConnection();
   void newConnection_aux();
   void disconnected();
   void disconnected_aux();
   void readyRead();
   void readyRead_aux();

   void HandlepushButton();
   void HandlepushButton_2();
   void mytimerEvent();
   void checkbox(bool);

private:
    Ui::MainWindow *ui;
    int m_timerId;
    QTcpServer *server, *server_aux;
    QTcpSocket *socket, *socket_aux;
    QHash<QTcpSocket*, QByteArray*> buffers; //We need a buffer to store data until block has completely received
    QHash<QTcpSocket*, qint32*> sizes; //We need to store the size to verify if a block has received completely
    QStringList fields;
    DataTable m_dataTable;
    int m_valueMax;
    int m_valueCount;
    QChartView * m_chart;
};

#endif // MAINWINDOW_H
