/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralWidget;
    QPushButton *Conectar_Bt;
    QPushButton *Enviar_Bt;
    QTextEdit *box_text;
    QPushButton *desconectar;
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(400, 193);
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName("centralWidget");
        Conectar_Bt = new QPushButton(centralWidget);
        Conectar_Bt->setObjectName("Conectar_Bt");
        Conectar_Bt->setGeometry(QRect(30, 70, 91, 31));
        Enviar_Bt = new QPushButton(centralWidget);
        Enviar_Bt->setObjectName("Enviar_Bt");
        Enviar_Bt->setEnabled(false);
        Enviar_Bt->setGeometry(QRect(220, 80, 91, 31));
        box_text = new QTextEdit(centralWidget);
        box_text->setObjectName("box_text");
        box_text->setEnabled(true);
        box_text->setGeometry(QRect(20, 20, 341, 41));
        box_text->setReadOnly(false);
        desconectar = new QPushButton(centralWidget);
        desconectar->setObjectName("desconectar");
        desconectar->setEnabled(false);
        desconectar->setGeometry(QRect(30, 100, 91, 31));
        MainWindow->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(MainWindow);
        menuBar->setObjectName("menuBar");
        menuBar->setGeometry(QRect(0, 0, 400, 22));
        MainWindow->setMenuBar(menuBar);
        mainToolBar = new QToolBar(MainWindow);
        mainToolBar->setObjectName("mainToolBar");
        MainWindow->addToolBar(Qt::ToolBarArea::TopToolBarArea, mainToolBar);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName("statusBar");
        MainWindow->setStatusBar(statusBar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        Conectar_Bt->setText(QCoreApplication::translate("MainWindow", "Conectar", nullptr));
        Enviar_Bt->setText(QCoreApplication::translate("MainWindow", "Pedir Info", nullptr));
        box_text->setPlaceholderText(QCoreApplication::translate("MainWindow", "Bienvenido", nullptr));
        desconectar->setText(QCoreApplication::translate("MainWindow", "Desconectar", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
