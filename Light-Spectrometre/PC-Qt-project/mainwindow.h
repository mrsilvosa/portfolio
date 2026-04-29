#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QMainWindow>

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

private slots:
    void on_Conectar_Bt_clicked();
    void on_Enviar_Bt_clicked();
    void on_desconectar_clicked();

    bool requestAndReadFloatVector(std::vector<float> &out, int expectedCount, int timeoutMs = 5000);
    static float readFloatLE(const uint8_t *b);
};

#endif // MAINWINDOW_H
