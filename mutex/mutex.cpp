#include <iostream>
#include <fstream>
#include <windows.h>

using namespace std;

struct BankAccount
{
    int accountNumber;
    int balance;
};

bool createFile(BankAccount& account)
{
    ofstream file("account.bin", ios::binary | ios::out);

    if (file.is_open())
    {
        account = { 10001, 10000 };
        file.write(reinterpret_cast<const char*>(&account), sizeof(BankAccount));
        file.close();

        if (!file)
        {
            cout << "Ошибка записи файла" << endl;
            return false;
        }
        return true;
    }
    else
    {
        cout << "Ошибка создания файла" << endl;
        return false;
    }
}

bool readFile(BankAccount& account)
{
    ifstream file("account.bin", ios::binary | ios::in);

    if (!file.is_open())
    {
        cout << "Файл не найден, создание файла" << endl;

        if (!createFile(account))
        {
            return false;
        }

        file.open("account.bin", ios::binary | ios::in);

        if (!file.is_open())
        {
            cout << "Ошибка повторного открытия файла" << endl;
            return false;
        }
    }

    file.read(reinterpret_cast<char*>(&account), sizeof(BankAccount));

    if (!file)
    {
        cout << "Ошибка чтения файла" << endl;
        file.close();
        return false;
    }

    file.close();
    return true;
}

HANDLE createMutex()
{
    HANDLE hmutex = CreateMutex(NULL, FALSE, L"Local\\MelekhovaMutex");

    if (!hmutex)
    {
        cout << "Ошибка создания мьютекса" << endl;
    }

    return hmutex;
}

bool waitMutex(BankAccount& account, HANDLE hmutex)
{
    DWORD result = WaitForSingleObject(hmutex, 3000);

    if (result == WAIT_OBJECT_0)
    {
        cout << "\nМьютекс успешно получен" << endl;
        return true;
    }
    else if (result == WAIT_ABANDONED)
    {
        if (readFile(account))
        {
            if (account.accountNumber == 10001 && account.balance >= 0)
            {
                return true;
            }
        }

        ReleaseMutex(hmutex);
        return false;
    }
    else if (result == WAIT_TIMEOUT)
    {
        cout << "Время ожидания мьютекса истекло" << endl;
        return false;
    }
    else
    {
        cout << "Не удалось получить мьютекс" << endl;
        return false;
    }
}

void releaseMutex(HANDLE hmutex)
{
    ReleaseMutex(hmutex);
}

bool writeFile(BankAccount& account)
{
    ofstream file("account.bin", ios::binary | ios::out);

    if (file.is_open())
    {
        file.write(reinterpret_cast<const char*>(&account), sizeof(BankAccount));
        file.close();

        if (!file)
        {
            cout << "Ошибка записи файла" << endl;
            return false;
        }

        return true;
    }
    else
    {
        cout << "Ошибка открытия файла для записи" << endl;
        return false;
    }
}

bool writeLog(BankAccount& account, const char* status, const char* operation, int startBalance, int amount)
{
    ofstream file("operations.log", ios::app);

    if (file.is_open())
    {
        DWORD PID = GetCurrentProcessId();

        file << "\nПроцесс: " << PID << endl
            << "Начальный баланс: " << startBalance << endl
            << "Операция: " << operation << endl
            << "Сумма операции: " << amount << endl
            << "Конечный баланс: " << account.balance << endl
            << "Статус: " << status << endl;

        file.close();

        if (!file)
        {
            cout << "Ошибка записи" << endl;
            return false;
        }

        return true;
    }
    else
    {
        cout << "Ошибка создания файла" << endl;
        return false;
    }
}

bool upBalance(BankAccount& account, int amount, HANDLE hmutex, int& newBalance)
{
    if (amount <= 0)
    {
        cout << "Нельзя пополнить на сумму меньше 1" << endl;
        return false;
    }

    if (waitMutex(account, hmutex))
    {
        if (!readFile(account))
        {
            releaseMutex(hmutex);
            return false;
        }

        if (account.accountNumber != 10001 || account.balance < 0)
        {
            cout << "Некорректные данные счета" << endl;
            releaseMutex(hmutex);
            return false;
        }

        int startBalance = account.balance;

        if (amount > INT_MAX - account.balance)
        {
            cout << "Счет превысит допустимый диапазон int" << endl;
            writeLog(account, "Ошибка: переполнение баланса", "Пополнение", startBalance, amount);
            releaseMutex(hmutex);
            return false;
        }

        account.balance += amount;

        if (!writeFile(account))
        {
            releaseMutex(hmutex);
            return false;
        }

        newBalance = account.balance;
        writeLog(account, "Успешно", "Пополнение", startBalance, amount);
        releaseMutex(hmutex);
        return true;
    }

    return false;
}

bool downBalance(BankAccount& account, int amount, HANDLE hmutex, int& newBalance)
{
    if (amount <= 0)
    {
        cout << "Нельзя снять сумму меньше 1" << endl;
        return false;
    }

    if (waitMutex(account, hmutex))
    {
        if (!readFile(account))
        {
            releaseMutex(hmutex);
            return false;
        }

        if (account.accountNumber != 10001 || account.balance < 0)
        {
            cout << "Некорректные данные счета" << endl;
            releaseMutex(hmutex);
            return false;
        }

        int startBalance = account.balance;

        if (amount > account.balance)
        {
            cout << "Недостаточно средств на счете" << endl;
            writeLog(account, "Ошибка: недостаточно средств", "Снятие", startBalance, amount);
            releaseMutex(hmutex);
            return false;
        }

        account.balance -= amount;

        if (!writeFile(account))
        {
            releaseMutex(hmutex);
            return false;
        }

        newBalance = account.balance;
        writeLog(account, "Успешно", "Снятие", startBalance, amount);
        releaseMutex(hmutex);
        return true;
    }

    return false;
}

void printBalance(BankAccount& account)
{
    cout << "Номер счета: " << account.accountNumber << endl;
    cout << "Баланс: " << account.balance << endl;
}

void Operation(BankAccount& account, HANDLE hmutex)
{
    for (int i = 0; i < 20; i++)
    {
        int operation = 1 + rand() % 2;
        int amount = 100 + rand() % (1000 - 100 + 1);
        DWORD PID = GetCurrentProcessId();
        int newBalance = 0;
        bool result;

        if (operation == 1)
        {
            result = upBalance(account, amount, hmutex, newBalance);
        }
        else
        {
            result = downBalance(account, amount, hmutex, newBalance);
        }

        cout << "\nPID процесса: " << PID << endl;

        if (operation == 1)
            cout << "Операция: Пополнение" << endl;
        else
            cout << "Операция: Снятие" << endl;

        cout << "Сумма операции: " << amount << endl;

        if (result)
            cout << "Полученный баланс: " << newBalance << endl;
        else
            cout << "Операция не выполнена" << endl;

        if (i < 19)
        {
            Sleep(100 + rand() % (500 - 100 + 1));
        }
    }
}

int menu()
{
    int choice;
    cout << "\nВыберите действие:" << endl;
    cout << "1. Просмотр баланса" << endl;
    cout << "2. Пополнение счета" << endl;
    cout << "3. Снятие средств со счета" << endl;
    cout << "4. Автоматическое выполнение 20 операций" << endl;
    cout << "5. Выход" << endl;
    cin >> choice;

    return choice;
}

int main()
{
    setlocale(LC_ALL, "Russian");
    srand(time(NULL));

    BankAccount account;

    HANDLE hmutex = createMutex();

    if (hmutex == NULL)
    {
        cout << "Ошибка создания мьютекса" << endl;
        return 1;
    }

    int choice = menu();
    int amount;
    int newBalance;

    while (choice != 5)
    {
        switch (choice)
        {
        case 1:
            if (waitMutex(account, hmutex))
            {
                if (readFile(account))
                {
                    printBalance(account);
                }

                releaseMutex(hmutex);
            }
            break;
        case 2:
            cout << "Введите сумму для пополнения: ";

            if (cin >> amount)
            {
                upBalance(account, amount, hmutex, newBalance);
            }
            else
            {
                cout << "Неверный ввод" << endl;
            }
            break;
        case 3:
            cout << "Введите сумму для снятия: ";

            if (cin >> amount)
            {
                downBalance(account, amount, hmutex, newBalance);
            }
            else
            {
                cout << "Неверный ввод" << endl;
            }
            break;
        case 4:
            cout << "Выполнение 20 операций" << endl;
            Operation(account, hmutex);
            break;
        default:
            cout << "Неверный выбор" << endl;
            break;
        }
        choice = menu();
    }
    CloseHandle(hmutex);
    return 0;
}