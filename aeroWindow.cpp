#include "aeroWindow.h"
#include <QFile>
#include <QTextStream>
#include <QPainter>
#include <QtMath>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <bits/stdc++.h>
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QTextEdit>

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

    QPushButton *addBtn = new QPushButton("Добавить самолёт", this);
    addBtn->setGeometry(20, 60, 220, 30);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        this->addNewFlightDialog();
    });

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

    flights_.clear();

    while (true) {
        int start, duration;
        QString type, name, city;

        in >> start >> duration >> type >> name >> city;

        std::default_random_engine generator;
        std::normal_distribution<double> distribution(15.0, 5.0);
        double dispersion = distribution(generator);

        if (in.status() != QTextStream::Ok)
            break;

        if (type.isEmpty())
            break;

        bool landing = (type == "landing");

        if (landing)
            city = "Янташубе";

        flights_.append({ start, duration, landing, name, city, -1 });
    }

    file.close();

    struct Node {
        int finish;
        int runway;
        bool operator<(const Node &o) const {
            return finish > o.finish;
        }
    };

    QVector<Flight*> sorted;
    sorted.reserve(flights_.size());
    for (auto &f : flights_) sorted.append(&f);

    std::sort(sorted.begin(), sorted.end(),
              [](Flight *a, Flight *b) { return a->start < b->start; });

    std::priority_queue<Node> pq;
    int nextRunway = 0;

    for (Flight *f : sorted) {
        if (!pq.empty() && pq.top().finish <= f->start) {
            auto top = pq.top();
            pq.pop();
            f->runway = top.runway;
        } else {
            f->runway = nextRunway++;
        }
        if (f->runway < 0 || f->runway >= 10) continue;
        pq.push({ f->start + f->duration, f->runway });
    }

    runways_ = std::min(nextRunway, 10);
    stats_.resize(runways_);

    runwayPositions.clear();
    for (int r = 0; r < runways_; ++r)
        runwayPositions.append(r);
    for (auto &f : flights_) {
        if (f.runway < 0 || f.runway >= runways_) {
            f.runway = -1;
        }
    }

    flights_.erase(
        std::remove_if(flights_.begin(), flights_.end(),
                       [](const Flight &f){ return f.runway < 0; }),
        flights_.end()
        );
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

    // ⬇⬇⬇ ВОТ ЭТОТ БЛОК СЮДА ⬇⬇⬇
    if (simMinute_ >= 1439 && !endDialogShown_) {
        endDialogShown_ = true;

        QMessageBox msg;
        msg.setWindowTitle("Сутки завершены");
        msg.setText("Симуляция дошла до 23:59.\nЗапустить заново или выйти?");
        msg.setIcon(QMessageBox::Question);

        QPushButton *restartBtn = msg.addButton("Перезапустить", QMessageBox::AcceptRole);
        QPushButton *exitBtn    = msg.addButton("Выйти", QMessageBox::RejectRole);

        msg.exec();

        if (msg.clickedButton() == restartBtn) {
            // Перезапуск симуляции
            simMinute_ = 0;
            currentMinute_ = 0;
            totalSimTime_ = 0;
            active_.clear();

            flights_.clear();
            loadFlights(":/helpFiles/flights.txt");

            endDialogShown_ = false;
            return;
        } else {
            close();
            return;
        }
    }
    // ⬆⬆⬆ КОНЕЦ БЛОКА ⬆⬆⬆

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
    auto isRunwayBusy = [&](int r) {
        for (const auto &f : flights_) {
            if (f.runway == r) {
                if (simMinute_ >= f.start && simMinute_ < f.start + f.duration)
                    return f.landing ? 1 : 2;
            }
        }
        return 0;
    };
    for (int i = 0; i < runwayPositions.size(); ++i) {

        int r = runwayPositions[i];
        double x = pad + i * laneStep;

        int busy = isRunwayBusy(r);
        QColor col;

        if(busy == 0) col = QColor(80, 80, 80);
        else if(busy == 1) col = QColor(255, 120, 120);
        else col = QColor(Qt::darkMagenta);

        p.setPen(QPen(col, lineWidth));
        p.drawLine(QPointF(x, pad), QPointF(x, h - pad));

        p.setPen(Qt::white);
        p.setFont(QFont("Monospace", 9));
        p.drawText(QPointF(x - 20, h - 10), QString("R%1").arg(r + 1));
    }

    for (auto it = active_.begin(); it != active_.end();) {
        double t = (simMinute_ - it->start) / qMax(1, it->duration);
        if (t >= 1.0) { it = active_.erase(it); continue; }
        if (t < 0.0) { ++it; continue; }

        if (!it->landing) t = t * t;
        else t = 1.0 - (1.0 - t) * (1.0 - t);

        int i = runwayPositions.indexOf(it->runway);
        double x = pad + i * laneStep;
        int yStart = it->landing ? pad : h - pad;
        int yEnd = it->landing ? h - pad : pad;
        int y = (int)std::round(yStart + (yEnd - yStart) * t);

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
                p.drawLine(QPointF(x - 5, y - 10), QPointF(x + 5, y - 10));
            else
                p.drawLine(QPointF(x - 5, y + 10), QPointF(x + 5, y + 10));
        }

        ++it;
    }

    int totalMin = (int)std::floor(simMinute_);
    int hours = totalMin / 60;
    int minutes = totalMin % 60;
    QString timeStr = QString("Время: %1:%2 | Активных: %3 | Полос: %4 | Скорость: %5 минут в секунду")
                          .arg(hours, 2, 10, QChar('0'))
                          .arg(minutes, 2, 10, QChar('0'))
                          .arg(active_.size())
                          .arg(runways_)
                          .arg(minutesPerSecond_);
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
    // Собираем текст
    QString info;
    info += QString("Полоса R%1\n\n").arg(runway + 1);
    info += "Время | Самолёт        | Город         | Тип     | Длит. | Статус\n";
    info += "-------------------------------------------------------------------\n";

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

        QString name  = f.name.leftJustified(14, ' ');
        QString city  = f.city.leftJustified(12, ' ');
        QString typeS = type.leftJustified(7, ' ');

        info += QString("%1:%2 | %3 | %4 | %5 | %6 мин | %7\n")
                    .arg(h, 2, 10, QChar('0'))
                    .arg(m, 2, 10, QChar('0'))
                    .arg(name)
                    .arg(city)
                    .arg(typeS)
                    .arg(f.duration)
                    .arg(status);
    }
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(QString("Полоса R%1").arg(runway + 1));
    dlg->setMinimumSize(600, 500);

    QVBoxLayout *layout = new QVBoxLayout(dlg);

    QTextEdit *text = new QTextEdit(dlg);
    text->setReadOnly(true);
    text->setFont(QFont("Monospace", 10));
    text->setText(info);

    layout->addWidget(text);

    QPushButton *closeBtn = new QPushButton("Закрыть", dlg);
    layout->addWidget(closeBtn);

    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    dlg->exec();
}


void AeroWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F) {
        minutesPerSecond_ *= 2.0;
        if (minutesPerSecond_ > 64) minutesPerSecond_ = 64;
    }

    if (event->key() == Qt::Key_S) {
        minutesPerSecond_ *= 0.5;
        if (minutesPerSecond_ < 0.125) minutesPerSecond_ = 0.125;
    }

    event->accept();
}

void AeroWindow::addNewFlightDialog() {
    bool ok1, ok2;

    QString name = QInputDialog::getText(this, "Самолёт", "Название:", QLineEdit::Normal, "", &ok1);
    if (!ok1 || name.isEmpty()) return;

    QStringList types = {"takeoff", "landing"};
    QString type = QInputDialog::getItem(this, "Тип", "Тип манёвра:", types, 0, false, &ok1);
    if (!ok1) return;

    QString city;
    if (type == "landing") {
        city = "Янташубе";
    } else {
        city = QInputDialog::getText(this, "Город", "Куда следует:", QLineEdit::Normal, "", &ok1);
        if (!ok1 || city.isEmpty()) return;
    }

    int start = QInputDialog::getInt(this, "Старт", "Время начала манёвра:", 0, 0, 100000, 1, &ok1);
    if (!ok1) return;

    int duration = QInputDialog::getInt(this, "Длительность", "Длительность (мин):", 1, 1, 10000, 1, &ok1);
    if (!ok1) return;

    bool landing = (type == "landing");

    Flight f{ start, duration, landing, name, city, -1 };

    if (!assignRunwayForFlight(f)) {
        QMessageBox::warning(this, "Ошибка", "Самолёт нельзя добавить — нет свободных полос (максимум 10).");
        return;
    }

    if (!runwayPositions.contains(f.runway))
        runwayPositions.append(f.runway);


    flights_.append(f);

    if (f.runway >= stats_.size())
        stats_.resize(f.runway + 1);

    if (simMinute_ < f.start + f.duration)
        active_.append(f);
    update();
}


bool AeroWindow::assignRunwayForFlight(Flight &f)
{
    const int MAX_RUNWAYS = 10;

    QVector<int> runwayFinish(MAX_RUNWAYS, 0);

    for (const auto &x : flights_) {
        if (x.runway >= 0 && x.runway < MAX_RUNWAYS) {
            int end = x.start + x.duration;
            runwayFinish[x.runway] = std::max(runwayFinish[x.runway], end);
        }
    }
    for (int r = 0; r < runways_; r++) {
        if (runwayFinish[r] <= f.start) {
            f.runway = r;
            return true;
        }
    }

    if (runways_ < MAX_RUNWAYS) {
        f.runway = runways_;
        runways_++;
        return true;
    }

    return false;
}

