# FTP-Server

Реализовать FTP-сервер, используя технологию Boost ASIO и callback функции. Достаточно реализовать только анонимный доступ (без аутентификации), пассивный режим (PASV, соединение для передачи данных устанавливает клиент), только команды, необходимые для перемещения по дереву каталогов, получению списка файлов и скачке файлов в поточном двоичном режиме. Тестировать корректность с браузерами, еще не поддерживающими FTP (не Chrome) или специализированными FTP-клиентам, например, WinSCP
