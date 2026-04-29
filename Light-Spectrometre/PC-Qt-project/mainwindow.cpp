#include <QtGui>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qmessagebox.h"

#include <QHBoxLayout>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QThread>
#include <QFileDialog>

#include <cstring>
#include <iostream>

#include "globales.h"
#include "libusb.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->Conectar_Bt->setEnabled(true);
    ui->Enviar_Bt->setEnabled(false);
    ui->desconectar->setEnabled(false);
}

MainWindow::~MainWindow()
{
    delete ui;

    if(connected > 0)
    {
        libusb_free_device_list(devs,1);
        libusb_exit(ctx);
    }
}


void MainWindow::on_Conectar_Bt_clicked()
{
    signed int r;
    unsigned int cnt;
    unsigned int i;
    signed int j, k, p;

    connected = 1;

    r = libusb_init(&ctx);

    libusb_set_debug(ctx, 3); //set verbosity level to 3, as suggested in the documentation

    cnt = libusb_get_device_list(ctx,&devs);

    qDebug() << "Hay" << cnt << "dispositivos conectados";

    for (i = 0; i < cnt; i++)
    {
        r = libusb_get_device_descriptor(devs[i],&desc);

        if(r < 0)
        {
            qDebug() << "Error con el device descriptor del dispositivo";
        }

        if(desc.idProduct == MY_PRODUCT && desc.idVendor == MY_VENDOR)
        {
            Dispositivo = i;

            qDebug() << "Mi dispositivo es el N°" << i;
            qDebug() << "Tiene " << desc.bNumConfigurations << " cantidad de configuraciones";
            qDebug() << "El Vendor ID es el N°: " << Qt::hex << desc.idVendor;
            qDebug() << "El Product ID es el N°: " << Qt::hex << desc.idProduct;
            qDebug() << "Device Class: " << desc.bDeviceClass;
            qDebug() << "Serial Number: " << desc.iSerialNumber;


            libusb_get_config_descriptor(devs[Dispositivo], 0, &config);


             qDebug() <<"Interfaces: "<<(uint8_t)config->bNumInterfaces;

             for(j=0; j<(uint8_t )(config->bNumInterfaces); j++)
             {
                 inter = &config->interface[j];

                 qDebug()<<"Cantidad de conf. alternativas: "<<(uint8_t) (inter->num_altsetting);

                 for(k=0; k < (uint8_t) (inter->num_altsetting); k++)
                 {
                     interdesc = &inter->altsetting[k];

                     qDebug() << "Cant. de Interfaces: "<<(uint8_t )interdesc->bInterfaceNumber;

                     qDebug() << "Cant. de Endpoints: "<<(uint8_t )interdesc->bNumEndpoints;

                     for(p= 0 ; p<(uint8_t )interdesc->bNumEndpoints; p++)
                     {
                            epdesc = &interdesc->endpoint[p];

                            qDebug() << "Tipo de Descriptor: "<<(uint8_t )epdesc->bDescriptorType;

                            qDebug() << "Dirección de EP: "<< Qt::hex << (uint8_t )epdesc->bEndpointAddress;

                     }

                 }
             }

            libusb_free_config_descriptor(config);


            r = libusb_open(devs[Dispositivo],&dev_handle);

            if(r<0)
            {
                qDebug() << "Error" << r << "abriendo dispositivo";
                ui->box_text->setText("Error abriendo el dispositivo");
                return;
            }
            //libusb_set_auto_detach_kernel_driver(dev_handle, 1);
            // Recommended "Safe" Manual Detach
            if (libusb_kernel_driver_active(dev_handle, 0) == 1)
            {
                qDebug() << "driver active returned 1, detaching...";
                int g = libusb_detach_kernel_driver(dev_handle, 0);
                if (g != 0)
                {
                    // Handle error only if it's not "already detached"
                    qDebug() << "not already detached, error?";
                }
                else
                {
                    qDebug() << "creo que salio bien el detach";
                }
            }
            else
            {
                qDebug() << "Driver not active on beginning";
            }

            ui->Conectar_Bt->setEnabled(false);
            ui->Enviar_Bt->setEnabled(true);
            ui->desconectar->setEnabled(true);
            ui->box_text->setText("Conectado");
            return;

        }
    }
    ui->box_text->setText("No se encontró el dispositivo");
}

void MainWindow::on_Enviar_Bt_clicked()
{

    std::vector<float> v;
    const int expectedCount = 296;
    bool ok = requestAndReadFloatVector(v, expectedCount, 500);

    int actual_length;
    unsigned int Enviados;

    if(!ok)
    {
        qDebug() << "Error leyendo vector desde el dispositivo";
        return;
    }
    else
    {
        qDebug() << "lectura completada, creando csv...";
    }
    //Pedir nombre de archivo:
    QString defaultName = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_intensity.csv";
    QString path = QFileDialog::getSaveFileName(this, "Guardar CSV", defaultName, "CSV files (*.csv)");
    if(path.isEmpty())
    {
        qDebug() <<"Lectura completada, guardado cancelado por el usuario.";
        return;
    }
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "Error", "No se pudo abrir el archivo para escribir.");
        qDebug() <<"Error guardando CSV.";
        return;
    }
    QTextStream out(&f);
    out << "index,value\n";
    for(int i = 0; i < (int)v.size(); ++i)
    {
        out << i << "," << QString::number(v[i], 'g', 9) << "\n";
    }
    f.close();

    //labelStatus_->setText(QString("Vector guardado (%1 valores) en: \n%2").arg(v.size()).arg(path));
    qDebug() << "VEctor guardado (" << v.size() << " valores) en : \n" << path;
    ui->box_text->setText("Datos recibidos y guardados");
}

float MainWindow::readFloatLE(const uint8_t *b)
{
    float val;
    // littel-endian copy
    uint8_t tmp[4];
    tmp[0] = b[0];
    tmp[1] = b[1];
    tmp[2] = b[2];
    tmp[3] = b[3];
    std::memcpy(&val, tmp, sizeof(float));
    return val;
}

bool MainWindow::requestAndReadFloatVector(std::vector<float> &out, int expectedCount, int timeoutMs)
{
    // protocolo simple:
    // escribimos un informe (report id = 0) con el primer byte = 0x01 (comando -> pedir vector)
    // el dispositivo respondera con uno o varios reportes que contienen los bytes del vector (little endian floats)
    // acumulamos hasta expectedCount * 4 bytes o timeout

    const int reportSize = 64; // tamaño tipico; hid_read leera hasta esto
    int neededBytes = expectedCount * 4;
    int actual_length;
    signed int Enviados;
    std::vector<uint8_t> bufferAll;
    bufferAll.reserve(neededBytes);

    // send request:
    uint8_t req[64];
    memset(req, 0, sizeof(req));
    // en hidapi en muchas plataformas, req[0] es report-id (0 si se usa ninguno)
    req[0] = 0x01; // comando 1 -> pedir vector (esto debe coincidir con firmware)
    /* ---- ajustar al uso debido ----- */

    // 3. CHECK the current configuration before setting it
    int current_config = 99;
    libusb_get_configuration(dev_handle, &current_config);
    qDebug() << "current confing is " << current_config;
    if (current_config != 1)
    {
        int u = libusb_set_configuration(dev_handle, 1);
        if (u != 0)
        {
            // If it still fails with errno 32, the device might already be
            // ready enough to just proceed to claiming.
        }
    }

    libusb_claim_interface(dev_handle, 0);

    Enviados = libusb_interrupt_transfer(dev_handle , 0x01 , req , 64 , &actual_length , timeoutMs);
    //libusb_interrupt_transfer(dev_handle , 0x81 , RxData , sizeof (RxData) , &actual_length , 0);

    //int res = hid_write(deviceHandle_, req, 65);
    if(Enviados < 0)
    {
        qWarning("write_error");
        return false;
    }
    else
    {
        qDebug() << "recibi del stm " << actual_length << " bytes";
    }

    // ahora leemos repetidamente hasta obtener los bytes:
    QElapsedTimer timer;
    timer.start();
    //    usamos bloqueos con timeput parcial
    qDebug() << "inicio timer";
    while((int)bufferAll.size() < neededBytes && timer.elapsed() < timeoutMs)
    {
        // hid_read bloquea; preferimos hid_read_timeout si está disponible
        // hid_read_timeout de hidapi puede usarse, pero no siempre está expuesto en algunos builds.
        // intentaremos hid_read with a small blocking read and bail out con timeout global.
        uint8_t tmp[64];
        memset(tmp, 0, sizeof(tmp));
        int got = 0; //hid_read(deviceHandle_, tmp, reportSize); // bloquea
        qDebug() << "empiezo recepcion";
        got = libusb_interrupt_transfer(dev_handle , 0x81 , tmp , 64 , &actual_length , timeoutMs);
        qDebug() << "actual length de " << actual_length;
        if(got < 0)
        {
            qWarning("read error");
            return false;
        }
        else if(actual_length == 0)
        {
            // sin datos inmediatos; permitir que el loop vuelva a intentar hasta timeout
            QThread::msleep(5);
            qDebug() << "durmiendo";
            continue;
        }
        else
        {
            // agregamos got bytes
            bufferAll.insert(bufferAll.end(), tmp, tmp + actual_length);
            qDebug() << "inserto bytes";
        }
    }
    if((int)bufferAll.size() < neededBytes)
    {
        qWarning("No se recibieron suficientes bytes: %d / %d", (int)bufferAll.size(), neededBytes);
        return false;
    }
    qDebug() << "transformando lo recibido en float:";
    // parse floats little-endian
    out.resize(expectedCount);
    for(int i = 0; i < expectedCount; ++i)
    {
        const uint8_t *p = &bufferAll[i*4];
        float f = readFloatLE(p);
        out[i] = f;
    }
    qDebug() << "transformado listo";
    return true;
}

void MainWindow::on_desconectar_clicked()
{
    if (dev_handle)
    {
        // 1. Release the interface you claimed (usually 0)
        libusb_release_interface(dev_handle, 0);

        // 2. Re-attach kernel driver (optional, Linux specific)
        // If you detached the driver to gain control, this gives it back to the OS
        libusb_attach_kernel_driver(dev_handle, 0);

        // 3. Close the device handle
        libusb_close(dev_handle);
        dev_handle = nullptr; // Prevent double-closing
    }

    // 4. Update your UI (Disable the disconnect button, enable connect button)
    ui->box_text->setText("Desconectado");
    ui->Conectar_Bt->setEnabled(true);
    ui->Enviar_Bt->setEnabled(false);
    ui->desconectar->setEnabled(false);
}
