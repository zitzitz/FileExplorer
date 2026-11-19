#include <QApplication>
#include "MainWindow.h"

//   TCP-клиент сканера каталогов

//   Архитектура приложения
//   - MainWindow (View): отвечает только за отрисовку интерфейса, обработку действий
//     пользователя и отображение статусов подключения/ошибок.
//   - TcpClient (Network / Protocol): реализует асинхронную работу с QTcpSocket,.
//   - FileListModel (Model): реализация QAbstractTableModel для отображения
//     и форматирования списка файлов и папок в QTableView.
 

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}