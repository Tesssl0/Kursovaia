@echo off
echo ================================
echo   Компиляция backend (MSVC)
echo ================================

REM Путь к PostgreSQL
set PG_INC=D:\postgreSQL\include
set PG_LIB=D:\postgreSQL\lib
set PG_DLL=D:\postgreSQL\bin\libpq.dll

echo Используемые пути:
echo INCLUDE: %PG_INC%
echo LIB: %PG_LIB%
echo DLL: %PG_DLL%
echo.

REM Компиляция
cl main.cpp /EHsc /I "%PG_INC%" /link "%PG_LIB%\libpq.lib" ws2_32.lib /out:main.exe

IF EXIST main.exe (
    echo.
    echo main.exe успешно создан.

    echo.
    echo Запуск сервера...
    echo ================================
    main.exe
) else (
    echo.
    echo ОШИБКА: main.exe не создан.
)

pause
