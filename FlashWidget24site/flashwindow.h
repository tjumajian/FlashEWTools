#ifndef FLASHWINDOW_H
#define FLASHWINDOW_H

#include <QMainWindow>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QMouseEvent>
#include <QDebug>
#include <QPainter>
#include <QPolygon>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QPointer>
#include <QComboBox>

#include "triangle.h"

QT_BEGIN_NAMESPACE
namespace Ui { class FlashWindow; }
QT_END_NAMESPACE

class FlashThread;

class FlashWindow : public QMainWindow
{
    Q_OBJECT

public:
    FlashWindow(QWidget *parent = nullptr);
    ~FlashWindow();
    void createWidgets();
    void createMenu();
    void setupLayout();
    void logPrint(const char *format, ...);
    void logPrint2(const QString& message);
    void setArgFailed(bool argFailed);

    void createStatusBar();
    void setStatus(const QString &text);
    void onCloseButton(QPointer<FlashThread> closeThread);

private:
    Ui::FlashWindow *ui;
    bool selectSitesAll = false;
    QLabel *statusLabel; // 声明状态栏标签

    QPushButton* siteButtonSelectAll;
    QPushButton* flashButtonHelp;
    QPushButton* siteButtonCancel;
    QPushButton* flashButtonTest;
    QPushButton* siteButtonReset;
    QPushButton* flashButtonOpenUart;
    QPushButton* flashButtonInit;

    QPushButton* textButtonClear;
    QPushButton* textButtonCopy;
    QPushButton* textButtonSave;
    QPushButton *refreshUartButton;
    QComboBox *comboBox_uart1;
    QComboBox *comboBox_uart2;

    QTextEdit* textEdit;

    QList<QWidget*> flashWidgets;
    QList<QWidget*> selectedWidgets;
    QList<int> widgetsNumeber;

    QPointer<FlashThread> m_flashThread;
    // FlashThread* m_flashThread2;
    QString findBinFile();

    bool EWfailed = false;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    QWidget* spaceWidget(int x, int y);
    TriangleWidget* triangleWidget;

private slots:
    void onSelectAllButton();
    void onHelpButton();
    void onCancelButton();
    void onTestButton();
    void onResetButton();
    void onOpenUartButton();
    void onInitButton();

    void onClearButton();
    void onCopyButton();
    void onSaveButton();
    void onRefreshUartButton();

    void on_uartThread_completed();
    void onThreadFinished(bool is_finished);
};
#endif // FLASHWINDOW_H
