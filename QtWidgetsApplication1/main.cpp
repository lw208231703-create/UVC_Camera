#include "QtWidgetsApplication1.h"
#include "infra/LogManager.h"
#include "infra/UiStrings.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    setUiLanguage(UiLang::ZH);    // default to Chinese; change to UiLang::EN for English
    QApplication app(argc, argv);
    app.setApplicationName("UVC Camera");
    app.setApplicationVersion("1.0.0");

    QtWidgetsApplication1 window;
    window.show();

    LOG_INFO("Main window shown");

    return app.exec();
}
