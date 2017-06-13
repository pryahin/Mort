#ifndef CLOCK_H
#define CLOCK_H

#include <QLabel>

// TODO: Clock + timer widget
// ->

class Clock : public QLabel
{
    Q_OBJECT
public:
    enum State {
        normal,
        hover,
        succeed,
        failed
    };

    Clock(QWidget* parent = nullptr, State _state = normal);

    State getState() const {
        return state;
    }

    void draw();
    void setFocused();
    void deleteFocus();

    bool isFocused();

signals:
    void clicked();

protected:

private:
    State state;
};

#endif // CLOCK_H
