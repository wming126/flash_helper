/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "src/hexeditor.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QSplitter *splitterMain;
    QTabWidget *tabWidget;
    QWidget *tabSPI;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QComboBox *comboProgrammer;
    QLabel *label_3;
    QComboBox *comboSpeed;
    QHBoxLayout *horizontalLayout_chip;
    QLabel *label_chip;
    QComboBox *comboChip;
    QHBoxLayout *horizontalLayout_2;
    QLineEdit *lineFile;
    QPushButton *btnBrowse;
    QHBoxLayout *horizontalLayout_3;
    QPushButton *btnDetect;
    QPushButton *btnRead;
    QPushButton *btnWrite;
    QPushButton *btnErase;
    QWidget *tabEEPROM;
    QVBoxLayout *verticalLayout_eeprom;
    QHBoxLayout *horizontalLayout_eep;
    QLabel *label_eep_prog;
    QComboBox *comboEepromProg;
    QLabel *label_eep_chip;
    QComboBox *comboEepromChip;
    QGroupBox *groupBox_eep_file;
    QHBoxLayout *horizontalLayout_eep_file;
    QLineEdit *lineEepromFile;
    QPushButton *btnEepromBrowse;
    QHBoxLayout *horizontalLayout_eep_btns;
    QPushButton *btnEepromRead;
    QPushButton *btnEepromWrite;
    QPushButton *btnEepromErase;
    QSpacerItem *verticalSpacer_eep;
    QWidget *tabSetup;
    QVBoxLayout *verticalLayout_3;
    QGroupBox *groupBox;
    QVBoxLayout *verticalLayout_4;
    QLabel *lblUdevStatus;
    QLabel *lblPolkitStatus;
    QHBoxLayout *horizontalLayout_4;
    QPushButton *btnInstallRules;
    QPushButton *btnRemoveRules;
    QLabel *label_2;
    QSpacerItem *verticalSpacer;
    QWidget *editorContainer;
    QVBoxLayout *verticalLayout_ed;
    QHBoxLayout *horizontalLayout_editor_title;
    QLabel *label_editor;
    QSpacerItem *horizontalSpacer_ed;
    QPushButton *btnSaveFile;
    HexEditor *hexEditor;
    QTextEdit *textLog;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(850, 800);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName("verticalLayout");
        splitterMain = new QSplitter(centralwidget);
        splitterMain->setObjectName("splitterMain");
        splitterMain->setOrientation(Qt::Vertical);
        tabWidget = new QTabWidget(splitterMain);
        tabWidget->setObjectName("tabWidget");
        tabSPI = new QWidget();
        tabSPI->setObjectName("tabSPI");
        verticalLayout_2 = new QVBoxLayout(tabSPI);
        verticalLayout_2->setObjectName("verticalLayout_2");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label = new QLabel(tabSPI);
        label->setObjectName("label");

        horizontalLayout->addWidget(label);

        comboProgrammer = new QComboBox(tabSPI);
        comboProgrammer->setObjectName("comboProgrammer");

        horizontalLayout->addWidget(comboProgrammer);

        label_3 = new QLabel(tabSPI);
        label_3->setObjectName("label_3");

        horizontalLayout->addWidget(label_3);

        comboSpeed = new QComboBox(tabSPI);
        comboSpeed->setObjectName("comboSpeed");

        horizontalLayout->addWidget(comboSpeed);


        verticalLayout_2->addLayout(horizontalLayout);

        horizontalLayout_chip = new QHBoxLayout();
        horizontalLayout_chip->setObjectName("horizontalLayout_chip");
        label_chip = new QLabel(tabSPI);
        label_chip->setObjectName("label_chip");

        horizontalLayout_chip->addWidget(label_chip);

        comboChip = new QComboBox(tabSPI);
        comboChip->setObjectName("comboChip");
        comboChip->setEditable(true);

        horizontalLayout_chip->addWidget(comboChip);


        verticalLayout_2->addLayout(horizontalLayout_chip);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        lineFile = new QLineEdit(tabSPI);
        lineFile->setObjectName("lineFile");

        horizontalLayout_2->addWidget(lineFile);

        btnBrowse = new QPushButton(tabSPI);
        btnBrowse->setObjectName("btnBrowse");

        horizontalLayout_2->addWidget(btnBrowse);


        verticalLayout_2->addLayout(horizontalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        btnDetect = new QPushButton(tabSPI);
        btnDetect->setObjectName("btnDetect");

        horizontalLayout_3->addWidget(btnDetect);

        btnRead = new QPushButton(tabSPI);
        btnRead->setObjectName("btnRead");

        horizontalLayout_3->addWidget(btnRead);

        btnWrite = new QPushButton(tabSPI);
        btnWrite->setObjectName("btnWrite");

        horizontalLayout_3->addWidget(btnWrite);

        btnErase = new QPushButton(tabSPI);
        btnErase->setObjectName("btnErase");

        horizontalLayout_3->addWidget(btnErase);


        verticalLayout_2->addLayout(horizontalLayout_3);

        tabWidget->addTab(tabSPI, QString());
        tabEEPROM = new QWidget();
        tabEEPROM->setObjectName("tabEEPROM");
        verticalLayout_eeprom = new QVBoxLayout(tabEEPROM);
        verticalLayout_eeprom->setSpacing(12);
        verticalLayout_eeprom->setObjectName("verticalLayout_eeprom");
        horizontalLayout_eep = new QHBoxLayout();
        horizontalLayout_eep->setObjectName("horizontalLayout_eep");
        label_eep_prog = new QLabel(tabEEPROM);
        label_eep_prog->setObjectName("label_eep_prog");

        horizontalLayout_eep->addWidget(label_eep_prog);

        comboEepromProg = new QComboBox(tabEEPROM);
        comboEepromProg->setObjectName("comboEepromProg");

        horizontalLayout_eep->addWidget(comboEepromProg);

        label_eep_chip = new QLabel(tabEEPROM);
        label_eep_chip->setObjectName("label_eep_chip");

        horizontalLayout_eep->addWidget(label_eep_chip);

        comboEepromChip = new QComboBox(tabEEPROM);
        comboEepromChip->setObjectName("comboEepromChip");
        comboEepromChip->setEditable(true);

        horizontalLayout_eep->addWidget(comboEepromChip);


        verticalLayout_eeprom->addLayout(horizontalLayout_eep);

        groupBox_eep_file = new QGroupBox(tabEEPROM);
        groupBox_eep_file->setObjectName("groupBox_eep_file");
        horizontalLayout_eep_file = new QHBoxLayout(groupBox_eep_file);
        horizontalLayout_eep_file->setObjectName("horizontalLayout_eep_file");
        lineEepromFile = new QLineEdit(groupBox_eep_file);
        lineEepromFile->setObjectName("lineEepromFile");

        horizontalLayout_eep_file->addWidget(lineEepromFile);

        btnEepromBrowse = new QPushButton(groupBox_eep_file);
        btnEepromBrowse->setObjectName("btnEepromBrowse");

        horizontalLayout_eep_file->addWidget(btnEepromBrowse);


        verticalLayout_eeprom->addWidget(groupBox_eep_file);

        horizontalLayout_eep_btns = new QHBoxLayout();
        horizontalLayout_eep_btns->setObjectName("horizontalLayout_eep_btns");
        btnEepromRead = new QPushButton(tabEEPROM);
        btnEepromRead->setObjectName("btnEepromRead");
        btnEepromRead->setMinimumSize(QSize(0, 40));

        horizontalLayout_eep_btns->addWidget(btnEepromRead);

        btnEepromWrite = new QPushButton(tabEEPROM);
        btnEepromWrite->setObjectName("btnEepromWrite");
        btnEepromWrite->setMinimumSize(QSize(0, 40));

        horizontalLayout_eep_btns->addWidget(btnEepromWrite);

        btnEepromErase = new QPushButton(tabEEPROM);
        btnEepromErase->setObjectName("btnEepromErase");
        btnEepromErase->setMinimumSize(QSize(0, 40));

        horizontalLayout_eep_btns->addWidget(btnEepromErase);


        verticalLayout_eeprom->addLayout(horizontalLayout_eep_btns);

        verticalSpacer_eep = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_eeprom->addItem(verticalSpacer_eep);

        tabWidget->addTab(tabEEPROM, QString());
        tabSetup = new QWidget();
        tabSetup->setObjectName("tabSetup");
        verticalLayout_3 = new QVBoxLayout(tabSetup);
        verticalLayout_3->setObjectName("verticalLayout_3");
        groupBox = new QGroupBox(tabSetup);
        groupBox->setObjectName("groupBox");
        verticalLayout_4 = new QVBoxLayout(groupBox);
        verticalLayout_4->setObjectName("verticalLayout_4");
        lblUdevStatus = new QLabel(groupBox);
        lblUdevStatus->setObjectName("lblUdevStatus");

        verticalLayout_4->addWidget(lblUdevStatus);

        lblPolkitStatus = new QLabel(groupBox);
        lblPolkitStatus->setObjectName("lblPolkitStatus");

        verticalLayout_4->addWidget(lblPolkitStatus);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        btnInstallRules = new QPushButton(groupBox);
        btnInstallRules->setObjectName("btnInstallRules");
        btnInstallRules->setStyleSheet(QString::fromUtf8("background-color: #27ae60; color: white; font-weight: bold;"));

        horizontalLayout_4->addWidget(btnInstallRules);

        btnRemoveRules = new QPushButton(groupBox);
        btnRemoveRules->setObjectName("btnRemoveRules");

        horizontalLayout_4->addWidget(btnRemoveRules);


        verticalLayout_4->addLayout(horizontalLayout_4);


        verticalLayout_3->addWidget(groupBox);

        label_2 = new QLabel(tabSetup);
        label_2->setObjectName("label_2");
        label_2->setWordWrap(true);

        verticalLayout_3->addWidget(label_2);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_3->addItem(verticalSpacer);

        tabWidget->addTab(tabSetup, QString());
        splitterMain->addWidget(tabWidget);
        editorContainer = new QWidget(splitterMain);
        editorContainer->setObjectName("editorContainer");
        verticalLayout_ed = new QVBoxLayout(editorContainer);
        verticalLayout_ed->setObjectName("verticalLayout_ed");
        verticalLayout_ed->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_editor_title = new QHBoxLayout();
        horizontalLayout_editor_title->setObjectName("horizontalLayout_editor_title");
        label_editor = new QLabel(editorContainer);
        label_editor->setObjectName("label_editor");
        QFont font;
        font.setBold(true);
        label_editor->setFont(font);

        horizontalLayout_editor_title->addWidget(label_editor);

        horizontalSpacer_ed = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_editor_title->addItem(horizontalSpacer_ed);

        btnSaveFile = new QPushButton(editorContainer);
        btnSaveFile->setObjectName("btnSaveFile");

        horizontalLayout_editor_title->addWidget(btnSaveFile);


        verticalLayout_ed->addLayout(horizontalLayout_editor_title);

        hexEditor = new HexEditor(editorContainer);
        hexEditor->setObjectName("hexEditor");

        verticalLayout_ed->addWidget(hexEditor);

        splitterMain->addWidget(editorContainer);
        textLog = new QTextEdit(splitterMain);
        textLog->setObjectName("textLog");
        textLog->setMaximumSize(QSize(16777215, 16777215));
        textLog->setReadOnly(true);
        textLog->setStyleSheet(QString::fromUtf8("background-color: #1e1e1e; color: #ffffff; font-family: 'Monospace';"));
        splitterMain->addWidget(textLog);

        verticalLayout->addWidget(splitterMain);

        MainWindow->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "FlashHelper - BIOS & EEPROM Tool", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "Programmer:", nullptr));
        label_3->setText(QCoreApplication::translate("MainWindow", "Speed:", nullptr));
        label_chip->setText(QCoreApplication::translate("MainWindow", "Chip Model (Optional):", nullptr));
        lineFile->setPlaceholderText(QCoreApplication::translate("MainWindow", "Select BIOS file...", nullptr));
        btnBrowse->setText(QCoreApplication::translate("MainWindow", "Browse", nullptr));
        btnDetect->setText(QCoreApplication::translate("MainWindow", "Detect Chip", nullptr));
        btnRead->setText(QCoreApplication::translate("MainWindow", "Read Flash", nullptr));
        btnWrite->setText(QCoreApplication::translate("MainWindow", "Write Flash", nullptr));
        btnErase->setText(QCoreApplication::translate("MainWindow", "Erase Flash", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabSPI), QCoreApplication::translate("MainWindow", "SPI Flash", nullptr));
        label_eep_prog->setText(QCoreApplication::translate("MainWindow", "Programmer:", nullptr));
        label_eep_chip->setText(QCoreApplication::translate("MainWindow", "Chip Model:", nullptr));
        groupBox_eep_file->setTitle(QCoreApplication::translate("MainWindow", "Firmware File", nullptr));
        lineEepromFile->setPlaceholderText(QCoreApplication::translate("MainWindow", "Select EEPROM file...", nullptr));
        btnEepromBrowse->setText(QCoreApplication::translate("MainWindow", "Browse", nullptr));
        btnEepromRead->setText(QCoreApplication::translate("MainWindow", "Read EEPROM", nullptr));
        btnEepromWrite->setText(QCoreApplication::translate("MainWindow", "Write EEPROM", nullptr));
        btnEepromErase->setText(QCoreApplication::translate("MainWindow", "Erase EEPROM", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabEEPROM), QCoreApplication::translate("MainWindow", "EEPROM", nullptr));
        groupBox->setTitle(QCoreApplication::translate("MainWindow", "Permission & Hardware Access", nullptr));
        lblUdevStatus->setText(QCoreApplication::translate("MainWindow", "Udev Rules: Unknown", nullptr));
        lblPolkitStatus->setText(QCoreApplication::translate("MainWindow", "Polkit Policy: Unknown", nullptr));
        btnInstallRules->setText(QCoreApplication::translate("MainWindow", "Install Permission Rules (Fix No-Password)", nullptr));
        btnRemoveRules->setText(QCoreApplication::translate("MainWindow", "Remove Rules (Cleanup)", nullptr));
        label_2->setText(QCoreApplication::translate("MainWindow", "Note: Installing rules requires root password ONCE. After that, you can flash BIOS without being asked for a password.", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabSetup), QCoreApplication::translate("MainWindow", "System Setup", nullptr));
        label_editor->setText(QCoreApplication::translate("MainWindow", "\345\215\201\345\205\255\350\277\233\345\210\266\346\237\245\347\234\213:", nullptr));
        btnSaveFile->setText(QCoreApplication::translate("MainWindow", "Save Changes to File", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
