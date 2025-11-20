# API.md

## Flight
- **start**: int – время начала манёвра
- **duration**: int – длительность
- **landing**: bool – посадка/взлёт
- **name**: QString – название самолёта
- **city**: QString – город
- **runway**: int – номер полосы

## RunwayStats
- **takeoffs**: int
- **landings**: int
- **busyTime**: int

## AeroWindow
- **AeroWindow(QWidget*)** – конструктор
- **paintEvent(QPaintEvent*)** – отрисовка
- **keyPressEvent(QKeyEvent*)** – управление F/S

### Приватные методы
- **loadFlights(path)** – загрузка рейсов
- **spawnFlights(minute)** – активация рейсов
- **progress(flight)** – 0..1 прогресс
- **addNewFlightDialog()** – добавление самолёта
- **assignRunwayForFlight(f)** – назначение полосы (≤10)

### Слоты
- **onFrame()** – обновление кадра
- **showRunwaySelector()** – выбор полосы
- **showRunwayDetails(runway)** – статистика по полосе
