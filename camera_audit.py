#!/usr/bin/env python3
"""
Скрипт для проведения внутреннего аудита безопасности веб-интерфейса IP-камеры (Hikvision).
Предназначен для выявления камер с дефолтными или слабыми паролями в закрытом сегменте сети.
Использование только в авторизованных сетях!
"""

import requests
from requests.auth import HTTPBasicAuth
from typing import List, Tuple, Optional
import sys


def check_camera_credentials(
    ip_address: str,
    credentials_list: List[Tuple[str, str]],
    endpoint: str = "/ISAPI/System/deviceInfo",
    timeout: int = 5
) -> Optional[Tuple[str, str]]:
    """
    Проверяет учетные данные для доступа к веб-интерфейсу IP-камеры.
    
    Args:
        ip_address: IP-адрес камеры
        credentials_list: Список кортежей (логин, пароль) для проверки
        endpoint: API endpoint для запроса
        timeout: Таймаут запроса в секундах
    
    Returns:
        Кортеж (логин, пароль) если найдены рабочие учетные данные, иначе None
    """
    url = f"http://{ip_address}{endpoint}"
    
    for username, password in credentials_list:
        try:
            response = requests.get(
                url,
                auth=HTTPBasicAuth(username, password),
                timeout=timeout
            )
            
            if response.status_code == 200:
                print(f"[✓] Успешная авторизация! IP: {ip_address}")
                print(f"    Логин: {username}, Пароль: {password}")
                print(f"    Статус код: {response.status_code}")
                return username, password
            
            elif response.status_code == 401:
                print(f"[✗] Неверные учетные данные: {username}:{password} (401 Unauthorized)")
                continue
            
            else:
                print(f"[!] Неожиданный статус код {response.status_code} для {username}:{password}")
                continue
                
        except requests.exceptions.Timeout:
            print(f"[!] Таймаут соединения с {ip_address} при проверке {username}:{password}")
            continue
            
        except requests.exceptions.ConnectionError:
            print(f"[!] Ошибка соединения с {ip_address}. Устройство недоступно.")
            break
            
        except requests.exceptions.RequestException as e:
            print(f"[!] Произошла ошибка при запросе: {e}")
            break
    
    return None


def main():
    """Основная функция скрипта."""
    
    # Список учетных данных для проверки (добавьте свои варианты)
    credentials_list = [
        ("admin", "12345"),
        ("admin", "admin"),
        ("admin", "123456"),
        ("admin", "password"),
        ("admin", "hikvision"),
        ("admin", "hik12345+"),
        ("user", "user"),
        ("guest", "guest"),
        ("root", "root"),
        ("admin", ""),
    ]
    
    # Пример IP-адреса камеры (замените на актуальный)
    camera_ip = "192.168.1.64"
    
    print("=" * 60)
    print("Аудит безопасности IP-камеры (Hikvision)")
    print("=" * 60)
    print(f"Целевой IP-адрес: {camera_ip}")
    print(f"Количество проверяемых учетных данных: {len(credentials_list)}")
    print("=" * 60)
    
    result = check_camera_credentials(camera_ip, credentials_list)
    
    print("=" * 60)
    if result:
        print("РЕЗУЛЬТАТ: Найдены рабочие учетные данные!")
        print(f"Логин: {result[0]}, Пароль: {result[1]}")
        print("РЕКОМЕНДАЦИЯ: Немедленно смените пароль на устройстве!")
    else:
        print("РЕЗУЛЬТАТ: Рабочие учетные данные не найдены.")
        print("Устройство либо недоступно, либо использует надежные пароли.")
    print("=" * 60)


if __name__ == "__main__":
    # Проверка наличия библиотеки requests
    try:
        import requests
    except ImportError:
        print("Ошибка: Библиотека 'requests' не установлена.")
        print("Установите её командой: pip install requests")
        sys.exit(1)
    
    main()
