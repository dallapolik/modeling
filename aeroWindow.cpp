#include "aeroWindow.h"
#include <QFile>
#include <QTextStream>
#include <QPainter>
#include <QtMath>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>

AeroWindow::AeroWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setMinimumSize(1000, 560);
    setAutoFillBackground(false);

    planePm_ = QPixmap(":/helpFiles/plane.png");
    loadFlights(":/helpFiles/flights.txt");

    QPushButton *btn = new QPushButton("Статистика по полосе", this);
    btn->setGeometry(20, 20, 220, 30);
    connect(btn, &QPushButton::clicked, this, &AeroWindow::showRunwaySelector);

    frameTimer_.setInterval(16);
    connect(&frameTimer_, &QTimer::timeout, this, &AeroWindow::onFrame);
    clock_.restart();
    frameTimer_.start();
}

void AeroWindow::loadFlights(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть flights.txt");
        return;
    }

    QTextStream in(&file);
    QVector<int> busyUntil(100, 0);

    QString firstLine = in.readLine().trimmed();
    if (firstLine.startsWith("runways=")) {
        runways_ = firstLine.mid(QString("runways=").length()).toInt();
    } else {
        runways_ = 10;
        in.seek(0);
    }

    int half = qMax(1, runways_ / 2);

    while (!in.atEnd()) {
        int start, runway, duration;
        QString type;
        in >> start >> runway >> type >> duration;
        if (in.status() != QTextStream::Ok || type.isEmpty()) continue;

        bool landing = (type == "landing");

        if (landing && runway < half) runway += half;
        if (!landing && runway >= half) runway %= half;

        if (start < busyUntil[runway])
            start = busyUntil[runway];
        busyUntil[runway] = start + duration;

        flights_.append({start, runway, landing, duration});
    }

    file.close();
    stats_.resize(runways_);
}

void AeroWindow::onFrame() {
    double dtSec = clock_.restart() / 1000.0;
    simMinute_ += dtSec * minutesPerSecond_;
    totalSimTime_ = qMax(totalSimTime_, simMinute_);

    int newMinute = (int)std::floor(simMinute_);
    while (currentMinute_ <= newMinute) {
        spawnFlights(currentMinute_);
        ++currentMinute_;
    }

    update();
}

void AeroWindow::spawnFlights(int minute) {
    for (const auto &f : flights_) {
        if (f.start == minute) {
            active_.append(f);
            if (f.landing)
                stats_[f.runway].landings++;
            else
                stats_[f.runway].takeoffs++;
            stats_[f.runway].busyTime += f.duration;
        }
    }
}

double AeroWindow::progress(const Flight &f) const {
    double t = (simMinute_ - f.start) / qMax(1, f.duration);
    t = qBound(0.0, t, 1.0);

    if (!f.landing) {
        return t * t;
    } else {
        return 1.0 - (1.0 - t) * (1.0 - t);
    }
}

void AeroWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (!bgImg_.isNull()) p.drawImage(rect(), bgImg_);

    const int pad = 40;
    const int w = width();
    const int h = height();
    const int availWidth = w - 2 * pad;
    const double laneStep = availWidth / qMax(1, runways_ - 1);
    const double lineWidth = std::max(1.0, 10.0 - runways_ * 0.3);

    p.setPen(QPen(QColor(60, 60, 60), lineWidth));
    for (int r = 0; r < runways_; ++r) {
        double x = pad + r * laneStep;
        p.drawLine(QPointF(x, pad), QPointF(x, h - pad));

        p.setPen(Qt::white);
        p.setFont(QFont("Monospace", 9));
        p.drawText(QPointF(x - 20, h - 10), QString("R%1").arg(r + 1));
        p.setPen(QPen(QColor(60, 60, 60), lineWidth));
    }

    for (auto it = active_.begin(); it != active_.end();) {
        double t = (simMinute_ - it->start) / qMax(1, it->duration);
        if (t >= 1.0) { it = active_.erase(it); continue; }
        if (t < 0.0) { ++it; continue; }

        if (!it->landing) t = t * t;
        else t = 1.0 - (1.0 - t) * (1.0 - t);

        double x = pad + it->runway * laneStep;
        int yStart = it->landing ? pad : h - pad;
        int yEnd = it->landing ? h - pad : pad;
        int y = (int)std::round(yStart + (yEnd - yStart) * t);

        // тень, нужна, если вместо картинки эллипсоид
        /*p.setOpacity(0.25);
        p.setBrush(Qt::black);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(x + 8, y + 18), 15, 5);
        p.setOpacity(1.0);*/

        if (!planePm_.isNull()) {
            QTransform tr;
            tr.translate(x, y);

            if (it->landing)
                tr.rotate(180);

            tr.scale(0.2, 0.2);
            tr.translate(-planePm_.width() / 2.0, -planePm_.height() / 2.0);

            p.setTransform(tr);
            p.drawPixmap(0, 0, planePm_);
            p.resetTransform();
        } else {
            p.setBrush(it->landing ? QColor(255, 120, 120) : QColor(120, 200, 255));
            p.setPen(Qt::black);
            p.drawEllipse(QPointF(x, y), 10, 16);

            if (it->landing)
                p.drawLine(QPointF(x - 5, y - 10), QPointF(x + 5, y - 10)); // вниз
            else
                p.drawLine(QPointF(x - 5, y + 10), QPointF(x + 5, y + 10)); // вверх
        }

        ++it;
    }

    int totalMin = (int)std::floor(simMinute_);
    int hours = totalMin / 60;
    int minutes = totalMin % 60;
    QString timeStr = QString("Время: %1:%2 | Активных: %3 | Полос: %4")
                          .arg(hours, 2, 10, QChar('0'))
                          .arg(minutes, 2, 10, QChar('0'))
                          .arg(active_.size())
                          .arg(runways_);
    p.setPen(Qt::white);
    p.setFont(QFont("Monospace", 12));
    p.drawText(QRect(0, 0, w, 30), Qt::AlignCenter, timeStr);
}



void AeroWindow::showRunwaySelector() {
    QStringList items;
    for (int i = 0; i < runways_; ++i)
        items << QString("R%1").arg(i + 1);

    bool ok;
    QString choice = QInputDialog::getItem(this, "Выбор полосы",
                                           "Выберите полосу:", items, 0, false, &ok);
    if (ok && !choice.isEmpty()) {
        int runway = choice.mid(1).toInt() - 1;
        showRunwayDetails(runway);
    }
}

void AeroWindow::showRunwayDetails(int runway) {
    QString info = QString("Полоса R%1\n\n").arg(runway + 1);
    info += "Время  | Тип     | Длит. | Статус\n";
    info += "-----------------------------------\n";

    for (const auto &f : flights_) {
        if (f.runway != runway) continue;

        QString type = f.landing ? "посадка" : "взлёт";
        QString status;
        if (simMinute_ >= f.start + f.duration)
            status = "завершён";
        else if (simMinute_ >= f.start)
            status = "в процессе";
        else
            status = "ожидается";

        int h = f.start / 60, m = f.start % 60;
        info += QString("%1:%2  | %3 | %4 мин | %5\n")
                    .arg(h, 2, 10, QChar('0'))
                    .arg(m, 2, 10, QChar('0'))
                    .arg(type, -7)
                    .arg(f.duration, 2)
                    .arg(status);
    }

    QMessageBox::information(this, QString("Полоса R%1").arg(runway + 1), info);
}
