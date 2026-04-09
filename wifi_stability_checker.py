#!/usr/bin/env python3
"""
Скрипт для автоматизации проверки стабильности подключения к Wi-Fi сети.

Этот скрипт использует библиотеку pywifi для тестирования различных паролей
к определенной Wi-Fi сети. Он полезен для аудита безопасности собственных сетей,
позволяя проверить, какие из сохраненных или предполагаемых паролей действительны.

Важно: Используйте этот скрипт только на сетях, которыми вы владеете или имеете
право тестировать. Несанкционированный доступ к чужим сетям незаконен.

ПРИМЕЧАНИЕ: Для работы скрипта требуется:
1. Реальный Wi-Fi адаптер в системе
2. Запуск от имени root/administrator
3. Установленный wpa_supplicant (для Linux)
"""

import time
import sys
import pywifi
from pywifi import const


# Глобальная переменная для режима эмуляции (тестирование без оборудования)
EMULATION_MODE = False


def create_wifi_profile(ssid: str, password: str) -> pywifi.Profile:
    """
    Создает объект профиля Wi-Fi с заданными параметрами.
    
    Объект Profile в pywifi представляет собой конфигурацию подключения,
    включая имя сети (SSID), пароль, тип шифрования и другие параметры.
    
    Args:
        ssid: Название целевой Wi-Fi сети
        password: Пароль для подключения
        
    Returns:
        Настроенный объект профиля для подключения
    """
    profile = pywifi.Profile()
    profile.ssid = ssid  # Имя целевой сети
    profile.auth = const.AUTH_ALG_OPEN  # Открытый метод аутентификации (стандарт для WPA/WPA2)
    profile.akm.append(const.AKM_TYPE_WPA2PSK)  # Используем WPA2-PSK (наиболее распространенный)
    profile.cipher = const.CIPHER_TYPE_CCMP  # Тип шифрования CCMP (AES)
    profile.key = password  # Ключ доступа (пароль)
    
    return profile


def test_wifi_connection(ssid: str, passwords_file: str, delay: int = 3, emulation: bool = False) -> None:
    """
    Тестирует подключение к Wi-Fi сети используя список паролей из файла.
    
    Функция последовательно пробует каждый пароль из файла, создавая временный
    профиль для каждой попытки. Это полезно для аудита безопасности, так как
    позволяет выявить слабые или устаревшие пароли, которые все еще могут 
    использоваться в организации.
    
    Автоматическая проверка нескольких конфигураций полезна потому что:
    1. Позволяет быстро протестировать множество вариантов паролей
    2. Помогает обнаружить забытые или не обновленные пароли
    3. Демонстрирует важность использования сложных уникальных паролей
    4. Позволяет проверить актуальность документации по сетевым доступам
    
    Args:
        ssid: Название целевой Wi-Fi сети
        passwords_file: Путь к файлу со списком паролей (по одному в строке)
        delay: Задержка между попытками в секундах (для стабилизации адаптера)
        emulation: Если True, работает в режиме эмуляции без реального оборудования
    """
    # Инициализация объекта PyWiFi
    # PyWiFi - это основной класс библиотеки, предоставляющий доступ 
    # к беспроводным интерфейсам системы
    wifi = pywifi.PyWiFi()
    
    # Получаем первый доступный беспроводной интерфейс
    # В системе может быть несколько сетевых адаптеров, поэтому выбираем первый активный
    interface = None
    try:
        interfaces = wifi.interfaces()
        if interfaces:
            interface = interfaces[0]
    except Exception as e:
        print(f"⚠️  Предупреждение: Не удалось получить список интерфейсов: {e}")
        if not emulation:
            print("❌ Ошибка: Не найден беспроводной сетевой адаптер!")
            print("\n💡 Решение:")
            print("   1. Убедитесь, что Wi-Fi адаптер подключен и включен")
            print("   2. Запустите скрипт от имени root/administrator")
            print("   3. Для Linux: установите wpa_supplicant")
            print("   4. Или используйте режим эмуляции: python script.py --emulate")
            return
    
    # Если интерфейс не найден, но включен режим эмуляции - работаем в демо-режиме
    if not interface and emulation:
        print("🔧 Режим эмуляции: работа без реального Wi-Fi адаптера")
        _run_emulation_test(ssid, passwords_file, delay)
        return
    
    if not interface:
        print("❌ Ошибка: Не найден беспроводной сетевой адаптер!")
        return
    
    print(f"📡 Используемый интерфейс: {interface.name()}")
    print(f"🎯 Целевая сеть: {ssid}")
    print(f"📄 Файл с паролями: {passwords_file}")
    print("-" * 60)
    
    # Чтение списка паролей из файла
    try:
        with open(passwords_file, 'r', encoding='utf-8') as f:
            passwords = [line.strip() for line in f if line.strip()]
    except FileNotFoundError:
        print(f"❌ Ошибка: Файл '{passwords_file}' не найден!")
        return
    except Exception as e:
        print(f"❌ Ошибка при чтении файла: {e}")
        return
    
    if not passwords:
        print("❌ Ошибка: Файл с паролями пуст!")
        return
    
    print(f"📊 Найдено паролей для проверки: {len(passwords)}")
    print("-" * 60)
    
    # Отключаемся от текущей сети перед началом тестирования
    # Это важно для чистоты эксперимента и избежания конфликтов подключений
    if interface.status() == const.IFACE_CONNECTED:
        print("⚠️  Обнаружено активное подключение. Отключаемся...")
        interface.disconnect()
        time.sleep(2)  # Ждем завершения отключения
    
    # Основной цикл тестирования паролей
    for index, password in enumerate(passwords, 1):
        print(f"\n🔑 Попытка #{index}: Проверка пароля '{password}'")
        
        # Создаем временный профиль для текущей попытки
        # Каждый новый профиль изолирован от предыдущих, что позволяет
        # тестировать разные конфигурации без влияния друг на друга
        profile = create_wifi_profile(ssid, password)
        
        # Удаляем существующие профили с таким же SSID во избежание конфликтов
        # Это гарантирует, что мы используем именно текущую конфигурацию
        interface.remove_all_network_profiles()
        
        # Добавляем созданный профиль в интерфейс
        interface.add_network_profile(profile)
        
        print("   🔄 Попытка подключения...")
        
        # Инициируем подключение используя текущий профиль
        interface.connect(profile)
        
        # Ждем некоторое время для установки соединения
        # Время ожидания зависит от скорости работы сетевого адаптера
        time.sleep(delay + 2)  # Дополнительное время на установление соединения
        
        # Проверяем статус подключения
        status = interface.status()
        
        if status == const.IFACE_CONNECTED:
            print(f"   ✅ УСПЕХ: Подключение установлено с паролем '{password}'")
            
            # Получаем информацию о подключении для дополнительной проверки
            # Это подтверждает, что соединение действительно активно
            try:
                connection_info = interface.network_profiles()
                print(f"   📋 Активные профили: {[p.ssid for p in connection_info]}")
            except:
                pass
                
            # Отключаемся после успешного теста для продолжения проверки
            interface.disconnect()
            time.sleep(delay)
        else:
            # Статус подключения не "CONNECTED", значит аутентификация не удалась
            # Возможные статусы: IFACE_DISCONNECTED, IFACE_CONNECTING, IFACE_AUTHENTICATING
            print(f"   ❌ НЕУДАЧА: Ошибка аутентификации или подключения")
            print(f"   📊 Текущий статус интерфейса: {status}")
            
            # Принудительно отключаемся на случай зависания в промежуточном статусе
            if status != const.IFACE_DISCONNECTED:
                interface.disconnect()
            
            # Задержка перед следующей попыткой
            # Критически важна для стабилизации работы сетевого адаптера
            # Без задержки адаптер может не успеть сбросить состояние
            time.sleep(delay)
        
        # Очищаем профили после каждой попытки для чистоты следующего теста
        interface.remove_all_network_profiles()
    
    print("\n" + "=" * 60)
    print("🏁 Тестирование завершено!")
    print("=" * 60)


def _run_emulation_test(ssid: str, passwords_file: str, delay: int) -> None:
    """
    Запускает эмуляцию тестирования Wi-Fi подключения без реального оборудования.
    
    Эта функция полезна для демонстрации работы скрипта и тестирования логики
    без необходимости наличия физического Wi-Fi адаптера.
    
    Args:
        ssid: Название целевой Wi-Fi сети
        passwords_file: Путь к файлу со списком паролей
        delay: Задержка между попытками в секундах
    """
    print(f"\n🎯 Целевая сеть (эмуляция): {ssid}")
    print(f"📄 Файл с паролями: {passwords_file}")
    print("-" * 60)
    
    # Чтение списка паролей из файла
    try:
        with open(passwords_file, 'r', encoding='utf-8') as f:
            passwords = [line.strip() for line in f if line.strip()]
    except FileNotFoundError:
        print(f"❌ Ошибка: Файл '{passwords_file}' не найден!")
        return
    except Exception as e:
        print(f"❌ Ошибка при чтении файла: {e}")
        return
    
    if not passwords:
        print("❌ Ошибка: Файл с паролями пуст!")
        return
    
    print(f"📊 Найдено паролей для проверки: {len(passwords)}")
    print("-" * 60)
    
    # Эмуляция цикла тестирования паролей
    for index, password in enumerate(passwords, 1):
        print(f"\n🔑 Попытка #{index}: Проверка пароля '{password}'")
        print("   🔄 Попытка подключения...")
        
        # Эмулируем задержку подключения
        time.sleep(min(delay, 1))  # Укороченная задержка для демо
        
        # В режиме эмуляции последний пароль считается успешным (для демонстрации)
        if index == len(passwords):
            print(f"   ✅ УСПЕХ (эмуляция): Подключение установлено с паролем '{password}'")
        else:
            print(f"   ❌ НЕУДАЧА (эмуляция): Ошибка аутентификации")
            print(f"   📊 Статус: IFACE_DISCONNECTED")
        
        # Задержка перед следующей попыткой
        time.sleep(delay)
    
    print("\n" + "=" * 60)
    print("🏁 Тестирование завершено (режим эмуляции)!")
    print("=" * 60)


def main():
    """
    Основная функция запуска скрипта.
    
    Здесь задаются параметры для тестирования:
    - SSID целевой сети
    - Путь к файлу с паролями
    - Задержка между попытками
    
    Для запуска скрипта:
    1. Создайте файл config_test.txt с паролями (по одному в строке)
    2. Измените переменную TARGET_SSID на название вашей сети
    3. Запустите скрипт с правами администратора/root
    4. Или используйте --emulate для режима эмуляции
    """
    
    # === КОНФИГУРАЦИЯ ===
    # Название целевой Wi-Fi сети для тестирования
    # Замените на имя вашей сети для проверки
    TARGET_SSID = "MyTestNetwork"
    
    # Путь к файлу со списком паролей для проверки
    # Формат файла: один пароль на строку
    PASSWORDS_FILE = "config_test.txt"
    
    # Задержка между попытками подключения в секундах
    # Рекомендуется 3-5 секунд для стабильной работы адаптера
    CONNECTION_DELAY = 3
    
    # Проверяем аргументы командной строки
    emulation_mode = '--emulate' in sys.argv or '-e' in sys.argv
    
    # === ЗАПУСК ТЕСТИРОВАНИЯ ===
    print("🚀 Запуск скрипта проверки Wi-Fi подключения...")
    if emulation_mode:
        print("🔧 Режим эмуляции (без реального оборудования)")
    else:
        print("⚠️  Требуется запуск от имени администратора/root!")
    print()
    
    test_wifi_connection(
        ssid=TARGET_SSID,
        passwords_file=PASSWORDS_FILE,
        delay=CONNECTION_DELAY,
        emulation=emulation_mode
    )


if __name__ == "__main__":
    main()
