/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QWidget *transportWidget;
    QHBoxLayout *horizontalLayout;
    QPushButton *loadButton;
    QPushButton *saveButton;
    QComboBox *displayModeCombo;
    QLabel *timeLabel;
    QLabel *bpmLabel;
    QLineEdit *bpmEdit;
    QSpacerItem *horizontalSpacer;
    QPushButton *loadButton1;
    QPushButton *saveButton1;
    QPushButton *playButton;
    QPushButton *stopButton;
    QScrollArea *timelineScroll;
    QWidget *timelineContent;
    QVBoxLayout *timelineLayout;
    QWidget *rulerWidget;
    QWidget *waveformWidget;
    QWidget *scrollBarWidget;
    QSpacerItem *verticalSpacer;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1200, 800);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        transportWidget = new QWidget(centralwidget);
        transportWidget->setObjectName("transportWidget");
        horizontalLayout = new QHBoxLayout(transportWidget);
        horizontalLayout->setObjectName("horizontalLayout");
        loadButton = new QPushButton(transportWidget);
        loadButton->setObjectName("loadButton");
        loadButton->setMinimumSize(QSize(32, 32));
        loadButton->setMaximumSize(QSize(32, 32));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/resources/icons/load.svg"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        loadButton->setIcon(icon);
        loadButton->setIconSize(QSize(24, 24));

        horizontalLayout->addWidget(loadButton);

        saveButton = new QPushButton(transportWidget);
        saveButton->setObjectName("saveButton");
        saveButton->setMinimumSize(QSize(32, 32));
        saveButton->setMaximumSize(QSize(32, 32));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/resources/icons/save.svg"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        saveButton->setIcon(icon1);
        saveButton->setIconSize(QSize(24, 24));

        horizontalLayout->addWidget(saveButton);

        displayModeCombo = new QComboBox(transportWidget);
        displayModeCombo->addItem(QString());
        displayModeCombo->addItem(QString());
        displayModeCombo->setObjectName("displayModeCombo");
        displayModeCombo->setMinimumWidth(100);

        horizontalLayout->addWidget(displayModeCombo);

        timeLabel = new QLabel(transportWidget);
        timeLabel->setObjectName("timeLabel");
        timeLabel->setMinimumWidth(100);
        timeLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout->addWidget(timeLabel);

        bpmLabel = new QLabel(transportWidget);
        bpmLabel->setObjectName("bpmLabel");

        horizontalLayout->addWidget(bpmLabel);

        bpmEdit = new QLineEdit(transportWidget);
        bpmEdit->setObjectName("bpmEdit");
        bpmEdit->setMaximumWidth(60);

        horizontalLayout->addWidget(bpmEdit);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        loadButton1 = new QPushButton(transportWidget);
        loadButton1->setObjectName("loadButton1");
        loadButton1->setMinimumSize(QSize(32, 32));
        loadButton1->setMaximumSize(QSize(32, 32));
        loadButton1->setIcon(icon);
        loadButton1->setIconSize(QSize(24, 24));

        horizontalLayout->addWidget(loadButton1);

        saveButton1 = new QPushButton(transportWidget);
        saveButton1->setObjectName("saveButton1");
        saveButton1->setMinimumSize(QSize(32, 32));
        saveButton1->setMaximumSize(QSize(32, 32));
        saveButton1->setIcon(icon1);
        saveButton1->setIconSize(QSize(24, 24));

        horizontalLayout->addWidget(saveButton1);

        playButton = new QPushButton(transportWidget);
        playButton->setObjectName("playButton");
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/resources/icons/play.svg"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        playButton->setIcon(icon2);
        playButton->setIconSize(QSize(24, 24));

        horizontalLayout->addWidget(playButton);

        stopButton = new QPushButton(transportWidget);
        stopButton->setObjectName("stopButton");
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icons/resources/icons/stop.svg"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        stopButton->setIcon(icon3);
        stopButton->setIconSize(QSize(24, 24));

        horizontalLayout->addWidget(stopButton);


        verticalLayout->addWidget(transportWidget);

        timelineScroll = new QScrollArea(centralwidget);
        timelineScroll->setObjectName("timelineScroll");
        timelineScroll->setWidgetResizable(true);
        timelineContent = new QWidget();
        timelineContent->setObjectName("timelineContent");
        timelineLayout = new QVBoxLayout(timelineContent);
        timelineLayout->setObjectName("timelineLayout");
        rulerWidget = new QWidget(timelineContent);
        rulerWidget->setObjectName("rulerWidget");
        rulerWidget->setMinimumHeight(30);

        timelineLayout->addWidget(rulerWidget);

        waveformWidget = new QWidget(timelineContent);
        waveformWidget->setObjectName("waveformWidget");
        waveformWidget->setMinimumHeight(250);

        timelineLayout->addWidget(waveformWidget);

        scrollBarWidget = new QWidget(timelineContent);
        scrollBarWidget->setObjectName("scrollBarWidget");
        scrollBarWidget->setMinimumHeight(75);
        scrollBarWidget->setMaximumHeight(75);

        timelineLayout->addWidget(scrollBarWidget);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        timelineLayout->addItem(verticalSpacer);

        timelineScroll->setWidget(timelineContent);

        verticalLayout->addWidget(timelineScroll);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1200, 22));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "DONTFLOAT", nullptr));
#if QT_CONFIG(tooltip)
        loadButton->setToolTip(QCoreApplication::translate("MainWindow", "\320\236\321\202\320\272\321\200\321\213\321\202\321\214 \321\204\320\260\320\271\320\273", nullptr));
#endif // QT_CONFIG(tooltip)
        loadButton->setText(QString());
#if QT_CONFIG(tooltip)
        saveButton->setToolTip(QCoreApplication::translate("MainWindow", "\320\241\320\276\321\205\321\200\320\260\320\275\320\270\321\202\321\214 \321\204\320\260\320\271\320\273", nullptr));
#endif // QT_CONFIG(tooltip)
        saveButton->setText(QString());
        displayModeCombo->setItemText(0, QCoreApplication::translate("MainWindow", "\320\222\321\200\320\265\320\274\321\217", nullptr));
        displayModeCombo->setItemText(1, QCoreApplication::translate("MainWindow", "\320\242\320\260\320\272\321\202\321\213", nullptr));

        timeLabel->setText(QCoreApplication::translate("MainWindow", "00:00:000", nullptr));
        bpmLabel->setText(QCoreApplication::translate("MainWindow", "BPM:", nullptr));
        bpmEdit->setText(QCoreApplication::translate("MainWindow", "120.00", nullptr));
#if QT_CONFIG(tooltip)
        loadButton1->setToolTip(QCoreApplication::translate("MainWindow", "\320\236\321\202\320\272\321\200\321\213\321\202\321\214 \321\204\320\260\320\271\320\273", nullptr));
#endif // QT_CONFIG(tooltip)
        loadButton1->setText(QString());
#if QT_CONFIG(tooltip)
        saveButton1->setToolTip(QCoreApplication::translate("MainWindow", "\320\241\320\276\321\205\321\200\320\260\320\275\320\270\321\202\321\214 \321\204\320\260\320\271\320\273", nullptr));
#endif // QT_CONFIG(tooltip)
        saveButton1->setText(QString());
        playButton->setText(QString());
        stopButton->setText(QString());
        waveformWidget->setStyleSheet(QCoreApplication::translate("MainWindow", "background-color: #2b2b2b;", nullptr));
        menubar->setStyleSheet(QCoreApplication::translate("MainWindow", "background-color: rgb(33, 33, 33);\n"
"color: rgb(255, 255, 255);", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
