#ifndef CONTROLLER_H
#define CONTROLLER_H

class Controller {
    public:
        Controller();
        ~Controller();

        virtual void loop() = 0;
        virtual void setup() = 0;

    private:
};



#endif
