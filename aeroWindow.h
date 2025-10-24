#pragma once
#include <QMainWindow>
#include <QVector>
#include <QTimer>
#include <QElapsedTimer>
#include <QPixmap>
#include <QImage>

struct Flight {
    int start;
    int runway;
    bool landing;
    int duration;
};

struct RunwayStats {
    int takeoffs = 0;
    int landings = 0;
    int busyTime = 0;
};

class AeroWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit AeroWindow(QWidget *parent = nullptr);

private slots:
    void onFrame();
    void showRunwaySelector();
    void showRunwayDetails(int runway);

private:
    void loadFlights(const QString &filename);
    void spawnFlights(int minute);
    double progress(const Flight &f) const;

    QVector<Flight> flights_;
    QVector<RunwayStats> stats_;
    QVector<Flight> active_;

    QTimer frameTimer_;
    QElapsedTimer clock_;
    double simMinute_ = 0;
    int currentMinute_ = 0;
    double minutesPerSecond_ = 1.0;
    int runways_ = 10;
    double totalSimTime_ = 0.0;

    QPixmap planePm_;
    QImage bgImg_;

protected:
    void paintEvent(QPaintEvent*) override;
};
