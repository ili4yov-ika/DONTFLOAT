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
    QPushButton *playButton;
    QPushButton *stopButton;
    QPushButton *metronomeButton;
    QPushButton *loopButton;
    QWidget *timelineContainer;
    QHBoxLayout *timelineContainerLayout;
    QScrollArea *timelineScroll;
    QWidget *timelineContent;
    QVBoxLayout *timelineLayout;
    QWidget *rulerWidget;
    QWidget *waveformWidget;
    QWidget *waveformVerticalScrollBarWidget;
    QWidget *scrollBarWidget;
    QWidget *pitchGridWidget;
    QWidget *pitchGridVerticalScrollBarWidget;
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

        horizontalLayout->addWidget(displayModeCombo);

        timeLabel = new QLabel(transportWidget);
        timeLabel->setObjectName("timeLabel");
        timeLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);

        horizontalLayout->addWidget(timeLabel);

        bpmLabel = new QLabel(transportWidget);
        bpmLabel->setObjectName("bpmLabel");

        horizontalLayout->addWidget(bpmLabel);

        bpmEdit = new QLineEdit(transportWidget);
        bpmEdit->setObjectName("bpmEdit");

        horizontalLayout->addWidget(bpmEdit, 0, Qt::AlignmentFlag::AlignLeft);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

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

        metronomeButton = new QPushButton(transportWidget);
        metronomeButton->setObjectName("metronomeButton");
        metronomeButton->setMinimumSize(QSize(32, 32));
        metronomeButton->setMaximumSize(QSize(32, 32));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/resources/icons/metronome.svg"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        metronomeButton->setIcon(icon4);
        metronomeButton->setIconSize(QSize(24, 24));
        metronomeButton->setCheckable(true);

        horizontalLayout->addWidget(metronomeButton);

        loopButton = new QPushButton(transportWidget);
        loopButton->setObjectName("loopButton");
        loopButton->setMinimumSize(QSize(32, 32));
        loopButton->setMaximumSize(QSize(32, 32));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/resources/icons/loop.svg"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
        loopButton->setIcon(icon5);
        loopButton->setIconSize(QSize(24, 24));
        loopButton->setCheckable(true);

        horizontalLayout->addWidget(loopButton);


        verticalLayout->addWidget(transportWidget);

        timelineContainer = new QWidget(centralwidget);
        timelineContainer->setObjectName("timelineContainer");
        timelineContainerLayout = new QHBoxLayout(timelineContainer);
        timelineContainerLayout->setObjectName("timelineContainerLayout");
        timelineScroll = new QScrollArea(timelineContainer);
        timelineScroll->setObjectName("timelineScroll");
        timelineScroll->setWidgetResizable(true);
        timelineContent = new QWidget();
        timelineContent->setObjectName("timelineContent");
        timelineContent->setGeometry(QRect(0, 0, 1162, 663));
        timelineLayout = new QVBoxLayout(timelineContent);
        timelineLayout->setObjectName("timelineLayout");
        rulerWidget = new QWidget(timelineContent);
        rulerWidget->setObjectName("rulerWidget");

        timelineLayout->addWidget(rulerWidget, 0, Qt::AlignmentFlag::AlignTop);

        waveformWidget = new QWidget(timelineContent);
        waveformWidget->setObjectName("waveformWidget");
        waveformVerticalScrollBarWidget = new QWidget(waveformWidget);
        waveformVerticalScrollBarWidget->setObjectName("waveformVerticalScrollBarWidget");
        waveformVerticalScrollBarWidget->setGeometry(QRect(1110, 0, 16, 16));

        timelineLayout->addWidget(waveformWidget, 0, Qt::AlignmentFlag::AlignTop);

        scrollBarWidget = new QWidget(timelineContent);
        scrollBarWidget->setObjectName("scrollBarWidget");

        timelineLayout->addWidget(scrollBarWidget);

        pitchGridWidget = new QWidget(timelineContent);
        pitchGridWidget->setObjectName("pitchGridWidget");
        pitchGridVerticalScrollBarWidget = new QWidget(pitchGridWidget);
        pitchGridVerticalScrollBarWidget->setObjectName("pitchGridVerticalScrollBarWidget");
        pitchGridVerticalScrollBarWidget->setGeometry(QRect(1120, 0, 20, 201));

        timelineLayout->addWidget(pitchGridWidget, 0, Qt::AlignmentFlag::AlignBottom);

        timelineScroll->setWidget(timelineContent);

        timelineContainerLayout->addWidget(timelineScroll);


        verticalLayout->addWidget(timelineContainer);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1200, 21));
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
        playButton->setText(QString());
        stopButton->setText(QString());
#if QT_CONFIG(tooltip)
        metronomeButton->setToolTip(QCoreApplication::translate("MainWindow", "\320\234\320\265\321\202\321\200\320\276\320\275\320\276\320\274", nullptr));
#endif // QT_CONFIG(tooltip)
        metronomeButton->setText(QString());
#if QT_CONFIG(tooltip)
        loopButton->setToolTip(QCoreApplication::translate("MainWindow", "\320\246\320\270\320\272\320\273", nullptr));
#endif // QT_CONFIG(tooltip)
        loopButton->setText(QString());
        waveformWidget->setStyleSheet(QCoreApplication::translate("MainWindow", "background-color: #2b2b2b;", nullptr));
        pitchGridWidget->setStyleSheet(QCoreApplication::translate("MainWindow", "background-color: #2b2b2b;", nullptr));
        menubar->setStyleSheet(QCoreApplication::translate("MainWindow", "background-color: rgb(33, 33, 33);\n"
"color: rgb(255, 255, 255);", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
