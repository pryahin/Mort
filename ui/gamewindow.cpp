#include "gamewindow.h"
#include "ui_gamewindow.h"

#include <QMessageBox>
#include <QFontDatabase>
#include <QFont>
#include <QTextStream>
#include <QMouseEvent>
#include <QLabel>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QColor>
#include <QThread>

#include "aboutdialog.h"

#include <QDebug>

QThread *workerThread;

QGraphicsDropShadowEffect* ShadowEffect(QGraphicsDropShadowEffect* eff)
{
    eff->setOffset(0, 0);
    eff->setBlurRadius(10.0);
    eff->setColor(QColor(24, 148, 197));
    return eff;
}

GameWindow::GameWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::GameWindow),
    user(User()),
    id_selected(-1),
    clockFacade(new ClockFacade()),
    times(QVector<QLabel*>(3)),
    timer_message(new Timer(nullptr, 60, 100)),
    loading(new QLabel(this))
{
    ui->setupUi(this);
    this->setWindowTitle("Mort");

    ui->stackedWidget->setCurrentIndex(0);
    ui->userLineEdit->setVisible(false);
    ui->pushButton->setVisible(false);

    int id = QFontDatabase::addApplicationFont(":/rsc/resources/ITCBLKAD.TTF");
    QString font_name = QFontDatabase::applicationFontFamilies(id).at(0);
    font = QFont(font_name, 38, QFont::Normal);

    times[0] = ui->time_1;
    times[1] = ui->time_2;
    times[2] = ui->time_3;

    QPixmap background(":/rsc/images/start_window2.png");
    background = background.scaled(ui->background->size(), Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    ui->background->setPixmap(background);
    ui->background->lower();

    connect(ui->actionAbout, &QAction::triggered, this, &GameWindow::about);
    connect(this, SIGNAL(clicked(int)), this, SLOT(launchGame(int)));

    drawBackground();
    drawLoading();

    // for firstPage
    ui->userLineEdit->setFont(font);
    ui->message->setFont(font);
    ui->pushButton->setFont(font);

    // for level page
    QFont fontLabel = QFont(font_name, 52, QFont::Normal);
    ui->timerLabel->setFont(fontLabel);
    QGraphicsDropShadowEffect *effTimer = new QGraphicsDropShadowEffect(this);
    ui->timerLabel->setGraphicsEffect(ShadowEffect(effTimer));
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::white);
    ui->timerLabel->setPalette(palette);

    //Music
    levelScenePlayer = new QMediaPlayer();

    QMediaPlaylist* messagePlayerPlaylist = new QMediaPlaylist();
    messagePlayerPlaylist->addMedia(QUrl("qrc:/music/resources/1.mp3"));
    messagePlayerPlaylist->setPlaybackMode(QMediaPlaylist::Loop);
    messagePlayer = new QMediaPlayer();
    messagePlayer->setPlaylist(messagePlayerPlaylist);
    messagePlayer->setVolume(50);

    QMediaPlaylist* backgroundPlayerPlaylist = new QMediaPlaylist();
    backgroundPlayerPlaylist->addMedia(QUrl("qrc:/music/resources/2.mp3"));
    backgroundPlayerPlaylist->setPlaybackMode(QMediaPlaylist::Loop);
    backgroundPlayer = new QMediaPlayer();
    backgroundPlayer->setPlaylist(backgroundPlayerPlaylist);
    backgroundPlayer->play();

    if (!user.exist())
    {
        messagePlayer->play();
        backgroundPlayer->setVolume(25);
        connect(timer_message, &Timer::timeout, this, &GameWindow::writeMessage);
        connect(ui->userLineEdit, SIGNAL(returnPressed()), ui->pushButton, SIGNAL(clicked()));
    }
    else
    {
        backgroundPlayer->setVolume(50);
        clockRead();
        drawClocks();
        drawShelf();
        drawLoading();
        ui->stackedWidget->setCurrentIndex(1);
    }
}

GameWindow::~GameWindow()
{
    delete messagePlayer;
    delete backgroundPlayer;
    delete levelScenePlayer;

    delete ui->timerLabel->graphicsEffect();
    delete ui->score->graphicsEffect();
    delete ui->label->graphicsEffect();

    if(user.getUsername() != "")
        clockWrite();

    //delete loading;
    delete timer_message;

    delete ui;
}

void GameWindow::about()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void GameWindow::launchGame(int id)
{
    qDebug() << "launch game " << id+1;
    idLevel = id;

    clockFacade->stop(id);
    startLoading();
}

void GameWindow::completedGame(int id)
{
    disconnect(scene, SIGNAL(levelFail(int)), this, SLOT(failedGame(int)));
    disconnect(scene, SIGNAL(levelComplete(int)), this, SLOT(completedGame(int)));

    clockFacade->succeed(id);
    times[id]->setText(clockFacade->clock_timers[id]->getDecoratedTime());
    times[id]->setStyleSheet("QLabel { color : green; }");
    user.setScore(user.getScore() + clockFacade->time(id)*10);
    ui->score->setText("Score: " + QString::number(user.getScore()));
    startLoading();
}

void GameWindow::failedGame(int id)
{
    disconnect(scene, SIGNAL(levelFail(int)), this, SLOT(failedGame(int)));
    disconnect(scene, SIGNAL(levelComplete(int)), this, SLOT(completedGame(int)));

    clockFacade->fail(id);
    times[id]->setText("0:00");
    times[id]->setStyleSheet("QLabel { color : red; }");
    startLoading();
    ui->stackedWidget->setFocus();
}

void GameWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (ui->stackedWidget->currentIndex() != 1)
        return;

    int select = clockFacade->press(id_selected, event->pos());
    if (select>=0)
        emit clicked(select);

    QMainWindow::mousePressEvent(event);
}

void GameWindow::keyPressEvent(QKeyEvent *event)
{
    if (ui->stackedWidget->currentIndex()!=0)
        return;
    else
        if (event->key() == Qt::Key_Space){
            timer_message->setInterval(10);
            ui->speedupLabel->setVisible(false);
        }
}

void GameWindow::drawClocks()
{
    clockFacade->draw();

    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::white);

    for (int i = 0; i < 3; i++) {
        times[i]->setFont(font);
        times[i]->setPalette(palette);

        if (clockFacade->getState(i) == _succeed) {
            times[i]->setStyleSheet("QLabel { color : green; }");
        }
        else if (clockFacade->time(i) <= 10) {
            times[i]->setStyleSheet("QLabel { color : red; }");
        }
        times[i]->setText(clockFacade->clock_timers[i]->getDecoratedTime());
    }

    QGraphicsDropShadowEffect *effLabel = new QGraphicsDropShadowEffect(this);
    QGraphicsDropShadowEffect *effScore = new QGraphicsDropShadowEffect(this);
    ui->label->setGraphicsEffect(ShadowEffect(effLabel));
    ui->score->setGraphicsEffect(ShadowEffect(effScore));

    ui->label->setFont(font);
    ui->label->setPalette(palette);
    ui->score->setFont(font);
    ui->score->setPalette(palette);

    ui->label->setText("Good night, " + user.getUsername() + ". They're waiting.");
    ui->score->setText("Score: " + QString::number(user.getScore()));
}

void GameWindow::drawLoading()
{
    QPixmap _loading(":/rsc/images/loading.png");
    loading->setGeometry(0, 0, 960, 540);
    _loading = _loading.scaled(960, 540,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    loading->setPixmap(_loading);
    loading->raise();
    loading->hide();
}

void GameWindow::drawBackground()
{
    QPixmap background(":/rsc/images/bg960x540.png");
    background = background.scaled(this->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPalette palette;
    palette.setBrush(QPalette::Background, background);
    this->setPalette(palette);
    this->setFixedSize(960, 540);
}

void GameWindow::drawShelf()
{
    QPixmap shelf(":/rsc/images/shelf.png");
    shelf = shelf.scaled(ui->shelf->size(), Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    ui->shelf->setPixmap(shelf);
}

void GameWindow::clockWrite()
{
    clockFacade->write();
}

void GameWindow::clockRead()
{
    clockFacade->read(this);
    loading->raise();
}

void GameWindow::writeMessage()  {
    QString message = "My sweet child! \n"
            "It's time for me to leave you alone. I was teaching you\n"
            "all your life and since now you finally have your chance\n"
            "to declare yourself as a true master. \n"
            "I proudly proclaim new Death: \n"
            "May the great Universe blessed you with luck.";

        if (ui->message->text().length() == message.length())
        {
            messagePlayer->stop();
            backgroundPlayer->setVolume(50);
            timer_message->stop();
            ui->userLineEdit->setVisible(true);
            ui->pushButton->setVisible(true);

        }
        else
        {
            ui->message->setText(ui->message->text() + message[ui->message->text().length()]);
        }
}

void GameWindow::endLoading()
{
    // "затухание"
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect();
    loading->setGraphicsEffect(eff);
    animationEnd = new QPropertyAnimation(eff, "opacity");
    animationEnd->setDuration(1700);
    animationEnd->setStartValue(1);
    animationEnd->setEndValue(0);
    animationEnd->setEasingCurve(QEasingCurve::OutBack);
    workerThread = new QThread();
    animationEnd->moveToThread(workerThread);

    switch(ui->stackedWidget->currentIndex())  {
         case 0:
             clockRead();
             clockWrite();
             drawBackground();
             drawShelf();
             drawClocks();
             ui->stackedWidget->setCurrentIndex(1);
             break;
         case 1:
             clockFacade->hide();
             view = new LevelView(ui->gameLevel);
             scene = new LevelScene(idLevel, view, ui->timerLabel, clockFacade->clock_timers[0], &user, levelScenePlayer);
             connect(scene, SIGNAL(levelFail(int)), this, SLOT(failedGame(int)));
             connect(scene, SIGNAL(levelComplete(int)), this, SLOT(completedGame(int)));
             ui->stackedWidget->setCurrentIndex(2);
             view->setScene(scene);
             view->setFocus();
             scene->getTimerAnimation()->start();
             break;
         case 2:
             clockFacade->show();
             ui->stackedWidget->setCurrentIndex(1);
             delete scene;
             delete view;
             break;
         }
    connect(workerThread, &QThread::started, this, &GameWindow::processLoading);
    workerThread->start();
}

void GameWindow::startLoading()
{
    drawLoading();
    // "появление"
    loading->show();
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
    loading->setGraphicsEffect(eff);
    animationStart = new QPropertyAnimation(eff,"opacity");
    animationStart->setDuration(1700);
    animationStart->setStartValue(0);
    animationStart->setEndValue(1);
    animationStart->setEasingCurve(QEasingCurve::InBack);
    animationStart->start(QPropertyAnimation::DeleteWhenStopped);
    connect(animationStart, &QPropertyAnimation::finished, this, &GameWindow::endLoading);
}

void GameWindow::clearAll()
{
    clockFacade->hide();
    ui->clock_1->hide();
    ui->clock_2->hide();
    ui->clock_3->hide();
    ui->time_1->hide();
    ui->time_2->hide();
    ui->time_3->hide();
    ui->label->hide();
    ui->score->hide();
    ui->shelf->hide();
}

void GameWindow::showAll()
{
    clockFacade->show();
    ui->clock_1->show();
    ui->clock_2->show();
    ui->clock_3->show();
    ui->time_1->show();
    ui->time_2->show();
    ui->time_3->show();
    ui->label->show();
    ui->score->show();
    ui->shelf->show();
}

void GameWindow::processLoading()
{
   animationEnd->start();
   while (animationEnd->state() != QPropertyAnimation::Stopped) {
     QCoreApplication::processEvents();
   }
   workerThread->quit();
   animationEnd->deleteLater();
   //workerThread->deleteLater();
}

void GameWindow::on_pushButton_clicked()
{
    if (ui->userLineEdit->text() == "")
        {
            ui->errorLabel->setText("You MUST have name!");
        }
    else if (ui->userLineEdit->text().length()>9)
    {
        ui->errorLabel->setText("Username must be less than 10 characters");
        ui->errorLabel->setStyleSheet("QLabel { color : red; }");
    }
    // TODO : регулярное выражение для всех форматов вида
    else if (ui->userLineEdit->text() == "death" || ui->userLineEdit->text() == "Death")
        {
            ui->errorLabel->setText("You MUST have name!");
        }
    else {
            user.setUsername(ui->userLineEdit->text());
            startLoading();
    }
}

void GameWindow::on_actionMute_triggered()
{
    if (ui->actionMute->isChecked())
    {
        backgroundPlayer->setMuted(true);
        messagePlayer->setMuted(true);
        levelScenePlayer->setMuted(true);
    }
    else
    {
        backgroundPlayer->setMuted(false);
        messagePlayer->setMuted(false);
        levelScenePlayer->setMuted(false);
    }
}
