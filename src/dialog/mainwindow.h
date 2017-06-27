﻿#ifndef _MAIN_WINDOW_H
#define _MAIN_WINDOW_H

#include "src/pch.h"

namespace Ui {
    class MainWindow;
}

class FitView;
class QGraphicsScene;
class QSystemTrayIcon;
class Server;
class QTextEdit;
class QToolButton;
class QGroupBox;
class RoomItem;
class ConnectionDialog;
class ConfigDialog;

class BroadcastBox : public QDialog
{
    Q_OBJECT

public:
    BroadcastBox(Server *server, QWidget *parent = 0);

protected:
    virtual void accept();

private:
    Server *server;
    QTextEdit *text_edit;
};

class BackLoader
{
public:
    static void preload();
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void setBackgroundBrush(bool center_as_origin);
	QGraphicsScene* getScene();
    void roundCorners();

#if defined(Q_OS_WIN) || defined(Q_OS_OSX) || (defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID))
    virtual void changeEvent(QEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
#endif

    bool isLeftPressDown;
    QPoint movePosition;

    static const int S_PADDING = 4;
    static const int S_CORNER_SIZE = 5;
    enum Direction
    {
        Up, Down, Left, Right, LeftTop, LeftBottom, RightTop, RightBottom, None = -1
    };

    bool isZoomReady;
    Direction direction;


protected:
    virtual void closeEvent(QCloseEvent *);


private:
    FitView *view;
    QGraphicsScene *scene;
    Ui::MainWindow *ui;
    ConnectionDialog *connection_dialog;
    ConfigDialog *config_dialog;
    QSystemTrayIcon *systray;
    Server *server;
    QPushButton *closeButton;

    void region(const QPoint &cursorGlobalPoint);

    void restoreFromConfig();

public slots:
    void startConnection();

private slots:
    void on_actionAbout_GPLv3_triggered();
    void on_actionAbout_Lua_triggered();
    void on_actionAbout_fmod_triggered();
    void on_actionReplay_file_convert_triggered();
    void on_actionRecord_analysis_triggered();
    void on_actionAcknowledgement_triggered();
    void on_actionBroadcast_triggered();
    void on_actionScenario_Overview_triggered();
    void on_actionRole_assign_table_triggered();
    void on_actionMinimize_to_system_tray_triggered();
    void on_actionShow_Hide_Menu_triggered();
    void on_actionFullscreen_triggered();
    void on_actionReplay_triggered();
    void on_actionAbout_triggered();
    void on_actionEnable_Hotkey_toggled(bool);
    void on_actionNever_nullify_my_trick_toggled(bool);
    void on_actionCard_Overview_triggered();
    void on_actionGeneral_Overview_triggered();
    void on_actionStart_Server_triggered();
    void on_actionExit_triggered();
    void on_actionCard_editor_triggered();

    void checkVersion(const QString &server_version, const QString &server_mod);
    void networkError(const QString &error_msg);
    void enterRoom();
    void gotoScene(QGraphicsScene *scene);
    void gotoStartScene();
    void enableDialogButtons();
    void startGameInAnotherInstance();
    void changeBackground();
    void on_actionView_ban_list_triggered();

    //
    void on_actionManage_Ban_IP_triggered();
};

#endif

