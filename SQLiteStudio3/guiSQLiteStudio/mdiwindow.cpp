#include "mdiwindow.h"
#include "mdichild.h"
#include "common/unused.h"
#include "mdiarea.h"
#include "mainwindow.h"
#include <QDateTime>
#include <QApplication>
#include <QDebug>
#include <QFocusEvent>
#include <QAction>
#include <QMessageBox>

MdiWindow::MdiWindow(MdiChild* mdiChild, MdiArea *mdiArea, Qt::WindowFlags flags) :
    QMdiSubWindow(mdiArea->viewport(), flags), mdiArea(mdiArea)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWidget(mdiChild);
}

MdiWindow::~MdiWindow()
{
    if (SQLITESTUDIO->getImmediateQuit())
        return;

    if (!MAINWINDOW->isClosingApp())
        MAINWINDOW->pushClosedWindowSessionValue(saveSession());

    mdiArea->windowDestroyed(this);
}

QVariant MdiWindow::saveSession()
{
    if (!widget())
        return QVariant();

    QHash<QString, QVariant> hash = getMdiChild()->getSessionValue().toHash();
    hash["title"] = windowTitle();
    hash["position"] = pos();
    hash["geometry"] = saveGeometry();
    return hash;
}

bool MdiWindow::restoreSession(const QVariant& sessionValue)
{
    if (!widget())
        return true;

    QHash<QString, QVariant> value = sessionValue.toHash();
    if (value.size() == 0)
        return true;

    if (value.contains("geometry"))
        restoreGeometry(value["geometry"].toByteArray());

    if (value.contains("position"))
        move(value["position"].toPoint());

    if (value.contains("title"))
    {
        QString title = value["title"].toString();
        rename(title);
    }

    return getMdiChild()->applySessionValue(sessionValue);
}

MdiChild* MdiWindow::getMdiChild() const
{
    if (!widget())
        return nullptr;

    return dynamic_cast<MdiChild*>(widget());
}

void MdiWindow::setWidget(MdiChild* value)
{
    QMdiSubWindow::setWidget(value);
    if (value)
        value->setMdiWindow(this);
}

bool MdiWindow::restoreSessionNextTime()
{
    return getMdiChild()->restoreSessionNextTime();
}

void MdiWindow::rename(const QString& title)
{
    setWindowTitle(title);

    QAction* task = mdiArea->getTaskByWindow(this);
    if (task)
        task->setText(title);
}

void MdiWindow::changeEvent(QEvent* event)
{
    if (event->type() != QEvent::WindowStateChange)
    {
        QMdiSubWindow::changeEvent(event);
        return;
    }

    QWindowStateChangeEvent *changeEvent = static_cast<QWindowStateChangeEvent *>(event);

    bool wasActive = changeEvent->oldState().testFlag(Qt::WindowActive);
    bool isActive = windowState().testFlag(Qt::WindowActive);

    /*
     * This is a hack for a bug in Qt: QTBUG-23515. The problem is that when QMdiSubWindow
     * changes its state (because we used shortcut, or the TaskBar to activate another window,
     * or window is maximized or minimized), then the code responsible for hiding old window
     * gives focus to its previous or next child widget (depending on what is the state change),
     * before the new window is shown. This seems to be happening in QWidgetPrivate::hide_helper().
     *
     */
    if (wasActive && isActive)
    {
        // Handle problem with maximize/minimize
        QWidget* w = focusWidget();
        QMdiSubWindow::changeEvent(event);
        if (w)
            w->setFocus();
    }
    else if (wasActive && !isActive)
    {
        // Handle problem with switching between 2 MDI windows - part 1
        lastFocusedWidget = focusWidget();
        QMdiSubWindow::changeEvent(event);
    }
    else if (!wasActive && isActive)
    {
        // Handle problem with switching between 2 MDI windows - part 2
        QMdiSubWindow::changeEvent(event);
        if (!lastFocusedWidget.isNull() && (!focusWidget() || !isAncestorOf(focusWidget())))
            lastFocusedWidget->setFocus();
    }
    else
        QMdiSubWindow::changeEvent(event);
}

void MdiWindow::closeEvent(QCloseEvent* e)
{
    if (MAINWINDOW->isClosingApp() || !getMdiChild()->isUncommited())
    {
        QMdiSubWindow::closeEvent(e);
        return;
    }

    int res = QMessageBox::question(this, tr("Uncommited changes"), getMdiChild()->getQuitUncommitedConfirmMessage(), tr("Don't close"), tr("Close anyway"));
    if (res == 1)
        QMdiSubWindow::closeEvent(e);
    else
        e->ignore();
}
