#include "bmwindow.h"

#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QStackedLayout>
#include <QMessageBox>
#include <QApplication>
#include <QProcess>
#include <QThread>
#include <QLabel>

#include <ddialog.h>
#include <DTitlebar>
#include <DPageIndicator>
#include <DApplication>

#include "view/isoselectview.h"
#include "view/usbselectview.h"
#include "view/progressview.h"
#include "view/resultview.h"

#include "bminterface.h"
#include "backend/bmhandler.h"

DWIDGET_USE_NAMESPACE

const static QString debugQSS = "background-color: rgba(255, 0, 255, 20%);border: 1px solid;"
                                " border-radius: 3; border-color: rgba(0, 255, 0, 20%);";

static void slideWidget(QWidget *left, QWidget *right)
{
    right->show();
    left->show();
    int delay = 300;
    QRect leftStart = QRect(0, 0, left->width(), left->height());
    QRect leftEnd = leftStart;
    leftEnd.setX(-left->width());

    QPropertyAnimation *animation = new QPropertyAnimation(left, "geometry");
    animation->setDuration(delay);
    animation->setStartValue(leftStart);
    animation->setEndValue(leftEnd);
    animation->start();

    QRect rightStart = QRect(left->width(), 0, right->width(), right->height());
    QRect rightEnd = leftStart;
    leftEnd.setX(0);

    QPropertyAnimation *animation2 = new QPropertyAnimation(right, "geometry");
    animation2->setDuration(delay);
    animation2->setStartValue(rightStart);
    animation2->setEndValue(rightEnd);
    animation2->start();

    animation->connect(animation, &QPropertyAnimation::finished,
                       animation, &QPropertyAnimation::deleteLater);
    animation2->connect(animation2, &QPropertyAnimation::finished,
                        animation2, &QPropertyAnimation::deleteLater);
    animation2->connect(animation2, &QPropertyAnimation::finished,
                        left, &QWidget::hide);

}

class BMWindowPrivate
{
public:
    BMWindowPrivate(BMWindow *parent): q_ptr(parent) {}

    QLabel          *m_title        = nullptr;
    QWidget         *actionWidgets  = nullptr;
    ISOSelectView   *isoWidget      = nullptr;
    UsbSelectView   *usbWidget      = nullptr;
    ProgressView    *progressWidget = nullptr;
    ResultView      *resultWidget   = nullptr;
//    DDialog         *warnDlg        = nullptr;

    BMInterface     *interface      = nullptr;

    BMWindow *q_ptr;
    Q_DECLARE_PUBLIC(BMWindow)
};

BMWindow::BMWindow(QWidget *parent)
    : BMWindowBaseClass(parent), d_ptr(new BMWindowPrivate(this))
{
    Q_D(BMWindow);

    setFixedSize(440, 550);

    d->interface = BMInterface::instance();

    // init about info
    QString descriptionText = tr("Deepin Boot Maker is a tool to make deepin boot disk developed by Deepin Technology Team. It's easy to operate with simple interface.");
    QString acknowledgementLink = "https://www.deepin.org/acknowledgments/deepin-boot-maker#thanks";
    qApp->setProductName(tr("Deepin Boot Maker"));
    qApp->setApplicationAcknowledgementPage(acknowledgementLink);
    qApp->setProductIcon(QPixmap(":/theme/light/image/deepin-boot-maker-96px.png").scaled(96, 96));
    qApp->setApplicationDescription(descriptionText);

    auto title = titlebar();
    auto flags = title->windowFlags() & ~Qt::WindowMaximizeButtonHint;
#ifndef Q_OS_LINUX
    flags = flags & ~Qt::WindowSystemMenuHint;
#endif
    title->setWindowFlags(flags);
    title->setTitle("");
    title->setIcon(QPixmap(":/theme/light/image/deepin-boot-maker.svg"));


    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);

#ifdef Q_OS_LINUX
    auto centralWidget = new QWidget;
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
#else
    setContentLayout(mainLayout);
#endif

    auto viewWidth = 440;
    auto *actionsLayout = new QStackedLayout;
    mainLayout->addLayout(actionsLayout);
    d->isoWidget = new ISOSelectView();
    d->isoWidget->setFixedSize(viewWidth, 476);

    d->usbWidget = new UsbSelectView;
    d->usbWidget->setFixedSize(viewWidth, 476);
    d->usbWidget->move(viewWidth, 0);

    d->progressWidget = new ProgressView;
    d->progressWidget->setFixedSize(viewWidth, 476);
    d->progressWidget->move(viewWidth, 0);

    d->resultWidget = new ResultView;
    d->resultWidget->setFixedSize(viewWidth, 476);
    d->resultWidget->move(viewWidth, 0);

    actionsLayout->addWidget(d->isoWidget);

    actionsLayout->addWidget(d->usbWidget);
    actionsLayout->addWidget(d->progressWidget);
    actionsLayout->addWidget(d->resultWidget);

    mainLayout->addSpacing(8);
    auto wsib = new DPageIndicator(this);
    wsib->setPageCount(3);
    wsib->setPointColor(QColor(44, 167, 248));
    wsib->setSecondaryPointColor(QColor(234, 238, 242));
    wsib->setCurrentPage(0);
    wsib->setFixedHeight(26);
    wsib->setPointRadius(3);
    wsib->setSecondaryPointRadius(3);
    wsib->setPointDistance(12);
    mainLayout->addWidget(wsib);

    connect(d->isoWidget, &ISOSelectView::isoFileSelected, this, [ = ] {
        slideWidget(d->isoWidget, d->usbWidget);
        setProperty("bmISOFilePath", d->isoWidget->isoFilePath());
        wsib->setCurrentPage(1);
    });

    connect(d->usbWidget, &UsbSelectView::deviceSelected, this, [ = ](const QString & partition, bool format) {
        auto flags = titlebar()->windowFlags() & ~Qt::WindowCloseButtonHint;
        flags &= ~Qt::WindowSystemMenuHint;
        flags &= ~Qt::WindowMaximizeButtonHint;
        titlebar()->setWindowFlags(flags);
        slideWidget(d->usbWidget, d->progressWidget);
        wsib->setCurrentPage(2);
        auto isoFilePath = property("bmISOFilePath").toString();
        qDebug() << "call interface install" << isoFilePath << partition << format;
        emit d->interface->startInstall(isoFilePath, "", partition, format);
    });

    // TODO: TEST Function
//        USBFormatError,
//        USBSizeError,
//        USBMountFailed,
//        ExtractImgeFailed,
    connect(d->progressWidget, &ProgressView::testCancel, this, [ = ] {
        setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);
        d->resultWidget->updateResult(BMHandler::SyscExecFailed, "title", "description");
//        d->resultWidget->updateResult(BMHandler::NoError, "title", "description");
        slideWidget(d->progressWidget, d->resultWidget);
        wsib->setCurrentPage(2);
    });

    connect(d->progressWidget, &ProgressView::finish,
    this, [ = ](quint32 error, const QString & title, const QString & description) {
        qDebug() << error << title << description;
        auto flags = titlebar()->windowFlags() & ~Qt::WindowMaximizeButtonHint;
#ifndef Q_OS_LINUX
        flags = flags & ~Qt::WindowSystemMenuHint;
#endif
        titlebar()->setWindowFlags(flags);
        d->resultWidget->updateResult(error, title, description);
        slideWidget(d->progressWidget, d->resultWidget);
        wsib->setCurrentPage(2);
    });

//    d->warnDlg = new Dtk::Widget::DDialog(this);
//    d->warnDlg->setWindowFlags(d->warnDlg->windowFlags() & ~Qt::WindowCloseButtonHint);
//    d->warnDlg->setFixedWidth(400);
//    d->warnDlg->setIcon(QMessageBox::standardIcon(QMessageBox::Warning));
//    d->warnDlg->setWindowTitle(tr("Format USB flash drive"));
//    d->warnDlg->setTextFormat(Qt::AutoText);
//    d->warnDlg->setMessage(tr("Please input root password."));
//    d->warnDlg->addButtons(QStringList() << tr("Retry") << tr("exit"));

//    d->isoWidget->hide();
//    emit d->isoWidget->isoFileSelected();
//    emit d->usbWidget->deviceSelected(",", false);
//    emit d->progressWidget->testCancel();
//    emit d->progressWidget->finish(0, "aa", "ccc");
    d->interface->start();
}

BMWindow::~BMWindow()
{

}

QString rootCommand()
{
    return QString("gksu \"%1  -d -n\"").arg(qApp->applicationFilePath());
}

void startBackend()
{
    QProcess::startDetached(rootCommand());
}

