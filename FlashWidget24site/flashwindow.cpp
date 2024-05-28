#include "flashwindow.h"
#include "ui_flashwindow.h"
#include "FlashThread.h"

#include <QScrollArea>
#include <QDialog>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QSerialPortInfo>
#include <QMessageBox>

FlashWindow::FlashWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::FlashWindow)
{
    setWindowIcon(QIcon(":/images/flash_ico"));
    ui->setupUi(this);
    createWidgets();
    createMenu();
    setupLayout();
    createStatusBar();

    connect(siteButtonSelectAll, &QPushButton::clicked, this, &FlashWindow::onSelectAllButton);
    connect(flashButtonHelp, &QPushButton::clicked, this, &FlashWindow::onHelpButton);
    connect(siteButtonCancel, &QPushButton::clicked, this, &FlashWindow::onCancelButton);
    connect(siteButtonReset, &QPushButton::clicked, this, &FlashWindow::onResetButton);
    connect(flashButtonTest, &QPushButton::clicked, this, &FlashWindow::onTestButton);
    connect(flashButtonOpenUart, &QPushButton::clicked, this, &FlashWindow::onOpenUartButton);
    connect(flashButtonInit, &QPushButton::clicked, this, &FlashWindow::onInitButton);

    connect(textButtonClear, &QPushButton::clicked, this, &FlashWindow::onClearButton);
    connect(textButtonCopy, &QPushButton::clicked, this, &FlashWindow::onCopyButton);
    connect(textButtonSave, &QPushButton::clicked, this, &FlashWindow::onSaveButton);
    connect(refreshUartButton, &QPushButton::clicked, this, &FlashWindow::onRefreshUartButton);
}

FlashWindow::~FlashWindow()
{
    delete ui;
    // 从 selectedWidgets 列表中移除控件
    selectedWidgets.clear();
    widgetsNumeber.clear();
    m_flashThread.clear();
    delete m_flashThread;
}

void FlashWindow::createWidgets()
{
    for (int i = 0; i < 24; ++i) {
        QWidget* widget = new QWidget(this);
        // 设置控件的样式和属性
        widget->setFixedSize(210, 180);
        widget->setGeometry(widget->x(), widget->y()+20, widget->width(), widget->height());

        widget->setObjectName("Site" + QString::number(i+1));
        widget->setStyleSheet("background-color: white;"
                              "color: Gray;"
                              "QLabel { color: Gray; }"
                              "QProgressBar { color: Gray; }");
        
        // 创建 TriangleWidget 窗口实例
        triangleWidget = new TriangleWidget(widget);
        triangleWidget->setObjectName("Triangle" + QString::number(i+1));

        // 创建 SiteName label
        QLabel* SiteNamelabel = new QLabel("Site " + QString::number(i+1), widget);
        SiteNamelabel->setAlignment(Qt::AlignCenter);
        SiteNamelabel->setGeometry(5, 5, 80, 16);
        QFont font = SiteNamelabel->font();
        font.setBold(true);
        font.setPointSize(13);
        SiteNamelabel->setFont(font);

        // pass label
        QLabel* passLabel = new QLabel("Pass:", widget);
        passLabel->setAlignment(Qt::AlignCenter);
        passLabel->setGeometry(10, 40, 35, 12);
        font = passLabel->font();
        font.setPointSize(9);
        passLabel->setFont(font);

        // passed text
        QTextEdit* passedText = new QTextEdit(widget);
        passedText->setObjectName("passedText" + QString::number(i+1));
        passedText->setGeometry(60, 40, 40, 15);
        passedText->setText(QString::number(0));
        passedText->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        // failed label
        QLabel* failedLabel = new QLabel("Failed:", widget);
        failedLabel->setAlignment(Qt::AlignCenter);
        failedLabel->setGeometry(10, 65, 45, 12);
        font = failedLabel->font();
        font.setPointSize(9);
        failedLabel->setFont(font);

        // failed text
        QTextEdit* failedText = new QTextEdit(widget);
        failedText->setObjectName("failedText" + QString::number(i+1));
        failedText->setGeometry(60, 65, 40, 15);
        failedText->setText(QString::number(0));
        failedText->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        // 创建进度条
        QProgressBar* progressBar = new QProgressBar(widget);
        progressBar->setObjectName("progressBar" +QString::number(i+1));
        progressBar->setGeometry(20, 160, 150,10);
        progressBar->setTextVisible(true);
        progressBar->setRange(0, 100);
        progressBar->setValue(50);

        flashWidgets.append(widget);
    }

}

void FlashWindow::setupLayout()
{
    // 创建网格布局
    QGridLayout* widgetLayout = new QGridLayout;
    for (int i = 0; i < 24; ++i) {
        widgetLayout->addWidget(flashWidgets.at(i), i / 6, i % 6);
    }
    // 创建滚动区域并设置网格布局
    QScrollArea* scrollArea = new QScrollArea;
    QWidget* scrollWidget = new QWidget;
    scrollWidget->setLayout(widgetLayout);
    scrollArea->setWidget(scrollWidget);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scrollArea->setStyleSheet("QScrollArea { border: none; }");
    // 创建四个按钮
    siteButtonSelectAll = new QPushButton("SelectAll");
    siteButtonCancel = new QPushButton("Cancel");
    siteButtonReset = new QPushButton("Reset");
    QHBoxLayout* siteButtonLayout = new QHBoxLayout;
    siteButtonLayout->addStretch(1);
    siteButtonLayout->addWidget(siteButtonSelectAll);
    siteButtonLayout->addStretch(1);
    siteButtonLayout->addWidget(siteButtonCancel);
    siteButtonLayout->addStretch(1);
    siteButtonLayout->addWidget(siteButtonReset);
    siteButtonLayout->addStretch(1);
    siteButtonLayout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    QFrame* frame = new QFrame;
    frame->setFrameStyle(QFrame::Box);  // 设置为矩形边框样式
    frame->setMidLineWidth(0);  // 设置中间线宽度为0
    frame->setLineWidth(1);
    frame->setStyleSheet("border-color: black;");
    QVBoxLayout* siteLayout = new QVBoxLayout;
    siteLayout->addWidget(scrollArea);
    siteLayout->addLayout(siteButtonLayout);
    frame->setLayout(siteLayout);

    // 创建文本框
    textEdit = new QTextEdit;
    textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn); // 始终显示垂直滚动条
    textEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    textEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QFont font("Arial", 12);
    textEdit->setFont(font);
    textEdit->setReadOnly(true);
    // 创建按钮
    textButtonClear = new QPushButton("Clear");
    textButtonCopy = new QPushButton("Copy");
    textButtonSave = new QPushButton("Save");
    // 创建水平布局并添加按钮
    QHBoxLayout* textButtonLayout = new QHBoxLayout;
    textButtonLayout->addWidget(textButtonClear);
    textButtonLayout->addWidget(textButtonCopy);
    textButtonLayout->addWidget(textButtonSave);
    // 创建布局管理器
    QVBoxLayout* textLayout = new QVBoxLayout;
    textLayout->addWidget(textEdit);
    textLayout->addWidget(spaceWidget(0, 10));
    textLayout->addLayout(textButtonLayout);
    textLayout->addWidget(spaceWidget(0, 10));


    // 创建水平布局并添加滚动区域和文本框
    QHBoxLayout* horizontalLayout = new QHBoxLayout;
    // horizontalLayout->addWidget(spaceWidget(30, 0));
    horizontalLayout->addWidget(frame, 3);
    horizontalLayout->addWidget(spaceWidget(30, 0));
    horizontalLayout->addLayout(textLayout, 1);
    horizontalLayout->addWidget(spaceWidget(30, 0));

    // 创建四个按钮
    flashButtonHelp = new QPushButton("Help");
    flashButtonOpenUart = new QPushButton("OpenUart");
    flashButtonInit = new QPushButton("Init");
    flashButtonTest = new QPushButton("Test");
    // 创建水平布局并添加按钮
    QHBoxLayout* flashButtonLayout = new QHBoxLayout;
    flashButtonLayout->addStretch(3);
    flashButtonLayout->addWidget(flashButtonOpenUart);
    flashButtonLayout->addStretch(1);
    flashButtonLayout->addWidget(flashButtonInit);
    flashButtonLayout->addStretch(1);
    flashButtonLayout->addWidget(flashButtonTest);
    flashButtonLayout->addStretch(1);
    flashButtonLayout->addWidget(flashButtonHelp);
    flashButtonLayout->addStretch(3);
    flashButtonLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
     // 创建布局管理器
    QVBoxLayout* spaceLayout = new QVBoxLayout;
    QHBoxLayout* uart1Layout = new QHBoxLayout;
    QLabel* uart1Label = new QLabel("uart1:");
    comboBox_uart1 = new QComboBox;
    comboBox_uart1->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    uart1Layout->addWidget(uart1Label);
    uart1Layout->addWidget(comboBox_uart1, 1);
    QHBoxLayout* uart2Layout = new QHBoxLayout;
    QLabel* uart2Label = new QLabel("uart2:");
    comboBox_uart2 = new QComboBox;
    comboBox_uart2->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    uart2Layout->addWidget(uart2Label);
    uart2Layout->addWidget(comboBox_uart2, 1);
    refreshUartButton = new QPushButton("Refresh");
    spaceLayout->addLayout(uart1Layout);
    spaceLayout->addLayout(uart2Layout);
    spaceLayout->addWidget(refreshUartButton);

    // 底部布局
    QHBoxLayout* flashLayout = new QHBoxLayout;
    flashLayout->addLayout(flashButtonLayout, 3);
    flashLayout->addWidget(spaceWidget(30, 0));
    flashLayout->addLayout(spaceLayout, 1);
    flashLayout->addWidget(spaceWidget(30, 0));

    // 创建垂直布局
    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(horizontalLayout); // 将水平布局添加到垂直布局
    mainLayout->addItem(new QSpacerItem(0, 30)); // 添加一个高度为20的空白部件
    mainLayout->addLayout(flashLayout); // 将水平布局添加到垂直布局
    mainLayout->addItem(new QSpacerItem(0, 30));

    // 创建中央窗口部件并设置布局
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
}



void FlashWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        for (QWidget* widget : flashWidgets) {
            QRect rect = widget->geometry();
            rect.translate(5, 30);
            if (rect.contains(pos)) {
                QString objectName = widget->objectName();
                int siteIndex  = (objectName.right(widget->objectName().length() - 4)).toInt();
                // qDebug() << siteIndex;
                TriangleWidget* sonTriangleWidget = widget->findChild<TriangleWidget*>("Triangle" + QString::number(siteIndex));

                QString currentStyleSheet = widget->styleSheet();
                QString expectedStyleSheet = "background-color: darkGray;"
                                             "color: black;"
                                             "QLabel { color: black; }"
                                             "QProgressBar { color: black; }";

                if (currentStyleSheet != expectedStyleSheet) {
                    // 如果控件未选中，则将其添加到列表中
                    if (!selectedWidgets.contains(widget)) {
                        selectedWidgets.append(widget);
                        widgetsNumeber.append(siteIndex);
                        qDebug() << "Widget selected:" << widget->objectName();  // 打印选中的widget
                        // qDebug() << "triangleWidget:" << triangleWidget->objectName();
                    }
                    //
                    widget->setStyleSheet(expectedStyleSheet);
                    if(sonTriangleWidget)
                    {
                        sonTriangleWidget->setTriangleColor(Qt::gray);
                        sonTriangleWidget->setTriangleName("NoTest");
                    }
                } else {
                    // 如果控件已选中，则从列表中移除
                    if (selectedWidgets.contains(widget)) {
                        selectedWidgets.removeOne(widget);
                        widgetsNumeber.removeOne(siteIndex);
                    }
                    //
                    widget->setStyleSheet("background-color: white;"
                                             "color: Gray;"
                                             "QLabel { color: Gray; }"
                                             "QProgressBar { color: Gray; }");
                    if(sonTriangleWidget)
                    {
                        sonTriangleWidget->setTriangleColor(Qt::white);
                        sonTriangleWidget->setTriangleName("NoTest");
                    }
                }
            }
        }
    }
}

void FlashWindow::createMenu()
{
    // 创建菜单栏
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // 创建菜单
    QMenu* fileMenu = menuBar->addMenu("File");
    QAction* openAction = fileMenu->addAction("Open");
    QAction* saveAction = fileMenu->addAction("Save");

    QMenu* editMenu = menuBar->addMenu("Edit");
    QAction* cutAction = editMenu->addAction("Cut");
    QAction* copyAction = editMenu->addAction("Copy");
    QAction* pasteAction = editMenu->addAction("Paste");

    QMenu* viewMenu = menuBar->addMenu("View");
    QAction* zoomInAction = viewMenu->addAction("Zoom In");
    QAction* zoomOutAction = viewMenu->addAction("Zoom Out");
}

QWidget* FlashWindow::spaceWidget(int x, int y)
{
    // 创建占位符控件
    QWidget* spacerWidget = new QWidget(this);
    spacerWidget->setFixedSize(x, y); // 设置大小为占位符的大小
    // spacerWidget->setVisible(false); // 设置为不可见

    return spacerWidget;
}

void FlashWindow::onSelectAllButton()
{
    logPrint("\nYou clicked selectAll button.");
    for (QWidget* widget : flashWidgets) {
        QString objectName = widget->objectName();
        int siteIndex  = (objectName.right(widget->objectName().length() - 4)).toInt();
        logPrint("Select site %d", siteIndex);
        // qDebug() << siteIndex;
        TriangleWidget* sonTriangleWidget = widget->findChild<TriangleWidget*>("Triangle" + QString::number(siteIndex));

        // 如果控件未选中，则将其添加到列表中
        if (!selectedWidgets.contains(widget)) {
            selectedWidgets.append(widget);
            widgetsNumeber.append(siteIndex);
            qDebug() << "Widget selected:" << widget->objectName();  // 打印选中的widget
            // qDebug() << "triangleWidget:" << triangleWidget->objectName();
        }
        //
        widget->setStyleSheet("background-color: darkGray;"
                                "color: black;"
                                "QLabel { color: black; }"
                                "QProgressBar { color: black; }");
        if(sonTriangleWidget)
        {
            sonTriangleWidget->setTriangleColor(Qt::gray);
            sonTriangleWidget->setTriangleName("NoTest");
        }
    }
}

void FlashWindow::onCancelButton()
{
    logPrint("\nYou clicked cancel select button.");
    for (QWidget* widget : flashWidgets) {
        QString objectName = widget->objectName();
        int siteIndex  = (objectName.right(widget->objectName().length() - 4)).toInt();
        // qDebug() << siteIndex;
        TriangleWidget* sonTriangleWidget = widget->findChild<TriangleWidget*>("Triangle" + QString::number(siteIndex));

        QString currentStyleSheet = widget->styleSheet();
        QString beforeStyleSheet = "background-color: darkGray;"
                                        "color: black;"
                                        "QLabel { color: black; }"
                                        "QProgressBar { color: black; }";
        QString afterStyleSheet = "background-color: white;"
                              "color: Gray;"
                              "QLabel { color: Gray; }"
                              "QProgressBar { color: Gray; }";

        if (currentStyleSheet == beforeStyleSheet) {
            // 如果控件已选中，则从列表中移除
            if (selectedWidgets.contains(widget)) {
                selectedWidgets.removeOne(widget);
                widgetsNumeber.removeOne(siteIndex);
            }
            // 设置控件及其子控件的灰色样式表
            widget->setStyleSheet(afterStyleSheet);
            if(sonTriangleWidget)
            {
                sonTriangleWidget->setTriangleColor(Qt::white);
                sonTriangleWidget->setTriangleName("NoTest");
            }
            logPrint("Cancel select site %d", siteIndex);
        }
    }
}

void FlashWindow::onResetButton()
{
    logPrint("\nYou  clicked reset button.");
    for (QWidget* widget : flashWidgets) {
        QString objectName = widget->objectName();
        int siteIndex  = (objectName.right(widget->objectName().length() - 4)).toInt();
        // qDebug() << siteIndex;
        TriangleWidget* sonTriangleWidget = widget->findChild<TriangleWidget*>("Triangle" + QString::number(siteIndex));
        QTextEdit* sonFailedText = widget->findChild<QTextEdit*>("failedText" + QString::number(siteIndex));
        QTextEdit* sonPassedText = widget->findChild<QTextEdit*>("passedText" + QString::number(siteIndex));
        QProgressBar* sonProgressBar = widget->findChild<QProgressBar*>("progressBar"+ QString::number(siteIndex));

        QString currentStyleSheet = widget->styleSheet();
        QString expectedStyleSheet = "background-color: white;"
                                        "color: Gray;"
                                        "QLabel { color: Gray; }"
                                        "QProgressBar { color: Gray; }";

        if (currentStyleSheet != expectedStyleSheet) {
            // 如果控件已选中，则从列表中移除
            if (selectedWidgets.contains(widget)) {
                selectedWidgets.removeOne(widget);
                widgetsNumeber.removeOne(siteIndex);
            }
            // 设置控件及其子控件的灰色样式表
            widget->setStyleSheet(expectedStyleSheet);
            if(sonTriangleWidget)
            {
                sonTriangleWidget->setTriangleColor(Qt::white);
                sonTriangleWidget->setTriangleName("NoTest");
            }
        }
        if(sonFailedText && sonFailedText->toPlainText() != QString::number(0))
        {
            sonFailedText->setText(QString::number(0));
        }
        if(sonPassedText && sonPassedText->toPlainText() != QString::number(0))
        {
            sonPassedText->setText(QString::number(0));
        }
        if(sonProgressBar && sonProgressBar->value() != 0)
        {
            sonProgressBar->setValue(30);
        }
        if(sonTriangleWidget)
        {
            sonTriangleWidget->setTriangleColor(Qt::white);
            sonTriangleWidget->setTriangleName("NoTest");
        }

    }
    // reset ft2232's GPIO
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QMainWindow::setEnabled(false);
    textEdit->setEnabled(false);
    if (m_flashThread)
    {
        m_flashThread->wait(); 
        m_flashThread.clear();
    }
    m_flashThread= new FlashThread;
    connect(m_flashThread, &FlashThread::logMessage, this, &FlashWindow::logPrint2);
    m_flashThread->resetPort();
    QCoreApplication::processEvents();
    m_flashThread->wait(); 
    m_flashThread.clear();
    QMainWindow::setEnabled(true);
    textEdit->setEnabled(true);
    QApplication::restoreOverrideCursor();
}

void FlashWindow::onOpenUartButton()
{
    logPrint("\nYou clicked openUart button.");
    if (!m_flashThread.isNull())
    {
        delete m_flashThread;
        m_flashThread.clear();
    }
    m_flashThread = new FlashThread;
    QDir sitedir = QCoreApplication::applicationDirPath();
    sitedir.mkdir("site_log");
    sitedir.cd("site_log");
    m_flashThread->setSiteDir(sitedir);

    if (comboBox_uart1->currentData().isNull() || comboBox_uart2->currentData().isNull())
    {
        QMessageBox::critical(this, "Error", "Please click refresh button to load uart port");
        // logPrint("Please click refresh button to load uart port");
        delete m_flashThread;
        m_flashThread.clear();
        return;
    }
    bool ok = m_flashThread->openUart(comboBox_uart1->currentData().toString(), comboBox_uart2->currentData().toString());
    if (ok)
    {
        logPrint("Opening...");
        logPrint("Open uart1: %s.\nOpen uart2: %s.", comboBox_uart1->currentData().toString().toStdString().c_str(), 
                                                    comboBox_uart2->currentData().toString().toStdString().c_str());
        logPrint("Open uart1&uart2 successfully");
        // flashButtonOpenUart->setText(tr("Close"));
        onCloseButton(m_flashThread);
    }
    else
    {
        logPrint("Open uart1: %s failed.\nOpen uart2: %s failed.", comboBox_uart1->currentData().toString().toStdString().c_str(), 
                                                                comboBox_uart2->currentData().toString().toStdString().c_str());
        logPrint("Open uart1&uart2 failed");
        delete m_flashThread;
        m_flashThread.clear();
    }

    connect(m_flashThread, &FlashThread::completed,
            this, &FlashWindow::on_uartThread_completed);


    logPrint("Close uart1&uart2 Automatically. NOTE: W/E flash don't need to open uart");
}

void FlashWindow::onCloseButton(QPointer<FlashThread> closeThread)
{
    // logPrint("\nYou clicked closeUart button.");
    bool ok = closeThread->closeUart();
    if (ok)
    {
        // logPrint("Close uart1&uart2 sucessfully");
        delete closeThread.data();
    }
}

void FlashWindow::onInitButton()
{
    logPrint("\nYou clicked init button.");
    m_flashThread = new FlashThread;
    bool ok = m_flashThread->boolInitLibFuncs();
    if (ok)
    {
        logPrint("Initial libMPSSE.dll successfully");
        logPrint("Initial libTunnel_64.dll successfully");
        logPrint("Initial SPI library Functions successfully");
        delete m_flashThread;
        m_flashThread.clear();
    }
    else{
        logPrint("Initial libMPSSE.dll failed");
        logPrint("Initial libTunnel_64.dll failed");
        logPrint("Initial SPI library Functions failed");
        delete m_flashThread;
        m_flashThread.clear();
    }
}

void FlashWindow::onTestButton()
{
    logPrint("\nYou clicked test button.");
    // for (int num = 0; num < widgetsNumeber.size(); num++)
    QString passedStyleSheet = "background-color: PaleGreen;"
                                        "color: black;"
                                        "QLabel { color: black; }"
                                        "QProgressBar { color: black; }";
    QString failedStyleSheet = "background-color: Bisque;"
                                        "color: black;"
                                        "QLabel { color: black; }"
                                        "QProgressBar { color: black; }";

    // 设置鼠标光标为等待状态
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QMainWindow::setEnabled(false);
    textEdit->setEnabled(false);
    for (QWidget* widget : selectedWidgets)
    {
        bool successed = false;
        QString objectName = widget->objectName();
        int siteIndex  = (objectName.right(widget->objectName().length() - 4)).toInt();
        qDebug()<< "siteIndex: "<<siteIndex;
        TriangleWidget* sonTriangleWidget = widget->findChild<TriangleWidget*>(
                                                        "Triangle" + QString::number(siteIndex));
        QTextEdit* sonFailedText = widget->findChild<QTextEdit*>(
                                                        "failedText" + QString::number(siteIndex));
        QTextEdit* sonPassedText = widget->findChild<QTextEdit*>(
                                                        "passedText" + QString::number(siteIndex));
        QProgressBar* sonProgressBar = widget->findChild<QProgressBar*>(
                                                        "progressBar"+ QString::number(siteIndex));

        if (m_flashThread)
        {
            m_flashThread->wait(); 
            m_flashThread->deleteLater();
            m_flashThread.clear();
        }
        if (sonTriangleWidget)
        {
            sonTriangleWidget->setTriangleColor(Qt::yellow);
            sonTriangleWidget->setTriangleName("Testing...");
        }

        m_flashThread= new FlashThread;
        connect(m_flashThread, &FlashThread::logMessage, this, &FlashWindow::logPrint2);
        connect(m_flashThread, &FlashThread::failedSignal, this, &FlashWindow::setArgFailed);
        // connect(m_flashThread, &FlashThread::manuFinish, this, &FlashWindow::onThreadFinished);
        QString m_binFile = findBinFile();
        if(m_binFile.isEmpty())
        {
            logPrint("ERROR! There is no <firmware>.bin file. Please add the file to the applicationDirPath");
            QMessageBox::critical(this, "Error", "There is no <firmware>.bin file. Please add the file to the applicationDirPath");
            break;
        }
        bool ok = m_flashThread->configSPI(1, siteIndex-1, m_binFile);
        QCoreApplication::processEvents();
        if (ok)
        {
            successed = true;
        }
        if (m_flashThread)
        {
            m_flashThread->wait();
            m_flashThread->deleteLater();
            // m_flashThread->terminate();
            m_flashThread.clear();
        }

        if(sonFailedText && successed && !EWfailed)
        {
            sonFailedText->setText(QString::number(0));
        }
        if(sonPassedText && successed && !EWfailed)
        {
            sonPassedText->setText(QString::number(1));
        }
        if(sonProgressBar && successed && !EWfailed)
        {
            sonProgressBar->setValue(100);
        }
        if(sonTriangleWidget && successed && !EWfailed)
        {
            sonTriangleWidget->setTriangleColor(Qt::green);
            sonTriangleWidget->setTriangleName("PASS");
        }

        if(successed && !EWfailed)
        {
            if (selectedWidgets.contains(widget)) {
                selectedWidgets.removeOne(widget);
                widgetsNumeber.removeOne(siteIndex);
            }
            widget->setStyleSheet(passedStyleSheet);
        }
        else
        {
            if (selectedWidgets.contains(widget)) {
                selectedWidgets.removeOne(widget);
                widgetsNumeber.removeOne(siteIndex);
            }
            sonFailedText->setText(QString::number(1));
            sonPassedText->setText(QString::number(0));
            sonProgressBar->setValue(1);
            sonTriangleWidget->setTriangleColor(Qt::red);
            sonTriangleWidget->setTriangleName("Failed");
            widget->setStyleSheet(failedStyleSheet);
        }
    }
    QMainWindow::setEnabled(true);
    textEdit->setEnabled(true);
    QApplication::restoreOverrideCursor();
}

void FlashWindow::onHelpButton()
{
    logPrint("\nYou clicked help button.");
    // 创建一个 QDialog 对象
    QDialog* helpDialog = new QDialog(this);
    helpDialog->setWindowTitle("Flash Erase/Write Help");

    // 创建 QTabWidget
    QTabWidget* tabWidget = new QTabWidget(helpDialog);

    // 第一页
    QWidget* helpPage = new QWidget();
    QVBoxLayout* layout1 = new QVBoxLayout(helpPage);
    QTextEdit* label1 = new QTextEdit(helpPage);
    label1->setText("使用手册 \n"
                    "1. 使用方法\n"
                    "    选择sites -> 点击Test按钮进行擦写 -> 查看结果\n"
                    "2. 界面介绍\n"
                    "    sites区: 可点击某个site进行选择或取消 \n"
                    "    sites区: 某site如果擦写成功: 三角区域会变绿、Pass的值为1、进度条为100%。否则: 三角区域会变红、Failed的值为1、进度条为0%\n"
                    "    log区: 软件右侧是log区, 用于输出测试的log\n"
                    "    按钮区: 见下面说明\n"
                    "3. 按钮解释\n"
                    "    SelectAll按钮:         选择所有site\n"
                    "    Cancel按钮:            取消选中的site\n"
                    "    Reset按钮:             重置所有site\n"
                    "    clear按钮:             清空log区域\n"
                    "    Copy按钮:              复制log区域文本\n"
                    "    Save按钮:              保存log区域为txt文件\n"
                    "    Refresh按钮:           获取串口资源(uart1 & uart2)\n"
                    "    OpenUart按钮:          只打开串口\n"
                    "    Init按钮:              只初始化DLL库和SPI库函数\n"
                    "    Test按钮:              擦写已选的sites\n"
                    "    Help按钮:              查看帮助\n"
                    "4. 注意\n"
                    "    软件使用完后请点击reset按钮, 以初始化FT2232、进行CP测试等"
                    );
    label1->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label1->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    layout1->addWidget(label1);

    // 第二页
    QWidget* aboutPage = new QWidget();
    QVBoxLayout* layout2 = new QVBoxLayout(aboutPage);
    QTextEdit* label2 = new QTextEdit(aboutPage);
    label2->setText("Tool Name:       Flash Erase/Write Tool\n"
                    "Version:         1.1\n"
                    "Author:          Jian Ma\n"
                    "Company:         Witmem Company\n"
                    "Email:           jian.ma@witintech.com\n"
                    "Address:         Tianjin\n"
                    "Introduction:    This tool helps erase/write one or more flash site you choose. You can click the site window to choose or cancel the testing sites. Also, clicking buttons below is equal.\n"
                    );
    label2->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label2->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    layout2->addWidget(label2);

    // 第三页
    QWidget* morePage = new QWidget();
    QVBoxLayout* layout3 = new QVBoxLayout(morePage);
    QTextEdit* label3 = new QTextEdit(morePage);
    label3->setText("How To Use commands to erase/write Flash in Cmd? \n"
                    "commands               explanation\n"
                    "set ad5                means set pin ad5 high level\n"
                    "clr ad5                means set pin ad5 low level\n"
                    "rst ac0                means reset ft2232\n"
                    "exit                   means exit the command line\n"
                    "erase fl0              means erase flash0\n"
                    "write fl0              means write flash0\n"
                    "read fl0               means read flash0\n"
                    "readid fl0             means read the flash0 device ID\n"
                    );
    label3->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label3->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    layout3->addWidget(label3);

    // 将页面添加到选项卡
    tabWidget->addTab(helpPage, "Help");
    tabWidget->addTab(morePage, "Commands");
    tabWidget->addTab(aboutPage, "About");

    // 创建 QDialog 的布局管理器
    QVBoxLayout* mainLayout = new QVBoxLayout(helpDialog);
    mainLayout->addWidget(tabWidget);

    // 设置 QDialog 的模态属性
    helpDialog->setModal(true);
    helpDialog->resize(500, 400);

    // 显示 QDialog
    helpDialog->show();
}

void FlashWindow::onClearButton()
{
    logPrint("\nYou clicked clear button.");
    textEdit->clear();
}

void FlashWindow::onCopyButton()
{
    logPrint("\nYou clicked copy button.");
    textEdit->selectAll();

    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(textEdit->toPlainText());
}


void FlashWindow::onSaveButton()
{
    logPrint("\nYou clicked save button.");
    QString currentPath = QDir::currentPath();

    // 打开文件对话框，选择保存文件的路径
    QString filePath = QFileDialog::getSaveFileName(this, "Save File", currentPath + "/FlashTestLog.txt", "Text Files (*.txt)");

    QFile file(filePath);

    if (!filePath.isEmpty() && file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream stream(&file);
        // 将textEdit的文本写入到文件中
        stream << textEdit->toPlainText();
        file.close();
    }
}

void FlashWindow::onRefreshUartButton()
{
    comboBox_uart1->clear();
    comboBox_uart2->clear();
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
    {
        comboBox_uart1->addItem(info.portName() + "  " + info.description(), info.portName());
        comboBox_uart2->addItem(info.portName() + "  " + info.description(), info.portName());
    }
}

void FlashWindow::createStatusBar()
{
    statusLabel = new QLabel(tr(" Ready."), this);
    statusBar()->addPermanentWidget(statusLabel, 1);

    QString styleSheet = "QStatusBar { background-color: white; }";
    statusBar()->setStyleSheet(styleSheet);

    QStatusBar *bar = statusBar();
    QSize size = bar->sizeHint();
    size.setHeight(30);
    bar->setSizeGripEnabled(false);
}

void FlashWindow::setStatus(const QString &text)
{
    statusLabel->setText(text);
}

void FlashWindow::logPrint(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    QString message = QString::vasprintf(format, args);
    va_end(args);
    textEdit->append(message);

    // 处理颜色
    QTextCursor cursor(textEdit->document());
    cursor.movePosition(QTextCursor::End);
    cursor.select(QTextCursor::BlockUnderCursor);

    if (message.contains("Error", Qt::CaseInsensitive) || message.contains("NULL", Qt::CaseInsensitive) || message.contains("fail", Qt::CaseInsensitive)) {
        QTextCharFormat format;
        format.setForeground(Qt::red);
        cursor.mergeCharFormat(format);
    }
}

void FlashWindow::logPrint2(const QString& message)
{
    QCoreApplication::processEvents();
    textEdit->append(message);

    // 处理颜色
    QTextCursor cursor(textEdit->document());
    cursor.movePosition(QTextCursor::End);
    cursor.select(QTextCursor::BlockUnderCursor);

    if (message.contains("Error", Qt::CaseInsensitive) || message.contains("NULL", Qt::CaseInsensitive)
                         || message.contains("fail", Qt::CaseInsensitive) || message.contains("失败")) {
        QTextCharFormat format;
        format.setForeground(Qt::red);
        cursor.mergeCharFormat(format);
    }
    if (message.contains("clear ac1.", Qt::CaseInsensitive)) {
        QTextCharFormat format;
        format.setForeground(Qt::blue);
        cursor.mergeCharFormat(format);
    }
    if (message.contains("成功")) {
        QTextCharFormat format;
        format.setForeground(Qt::green);
        cursor.mergeCharFormat(format);
    }

    QTextCharFormat purpleFormat;
    purpleFormat.setForeground(Qt::magenta);
    QRegularExpression hexRegex("\\b0x[0-9a-fA-F]+\\b");
    QTextCursor cursor2(textEdit->document());

    while (!cursor2.isNull()) {
        cursor2 = textEdit->document()->find(hexRegex, cursor2);
        if (!cursor2.isNull()) {
            cursor2.mergeCharFormat(purpleFormat);
        }
    }
}

void FlashWindow::setArgFailed(bool argFailed)
{
    QCoreApplication::processEvents();
    EWfailed = argFailed;
}

void FlashWindow::on_uartThread_completed()
{
    this->setEnabled(true);
    if (!m_flashThread.isNull())
    {
        delete m_flashThread.data();
        m_flashThread = nullptr;
    }
}

QString FlashWindow::findBinFile()
{
    QDir binPathDir = QCoreApplication::applicationDirPath();
    QStringList filter("*.bin");
    QStringList binFiles = binPathDir.entryList(filter, QDir::Files, QDir::Name);
    if (!binFiles.isEmpty())
    {
        QString binFilePath = binPathDir.filePath(binFiles.first());
        return binFilePath;
    }
    return "";
}

void FlashWindow::onThreadFinished(bool is_finished)
{
    // // 恢复窗口操作
    // setEnabled(is_finished);
    // // 恢复鼠标光标
    // QApplication::restoreOverrideCursor();
}