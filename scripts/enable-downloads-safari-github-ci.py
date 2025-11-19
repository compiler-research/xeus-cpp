import pyautogui
import time


def main():
    # Click Safari icon
    pyautogui.moveTo(150, 720, duration=1)
    pyautogui.click()
    time.sleep(1)
    # Click Safari Menu
    pyautogui.moveTo(60, 10, duration=1)
    pyautogui.click()
    time.sleep(1)
    # Click Settings
    pyautogui.moveTo(75, 102, duration=1)
    pyautogui.click()
    time.sleep(2.4)
    # Click websites page of settings
    pyautogui.moveTo(700, 240, duration=1)
    pyautogui.click()
    time.sleep(1.2)
    # Click Downloads section of webpages page
    pyautogui.moveTo(350, 630, duration=1)
    pyautogui.click()
    time.sleep(1.2)
    # Change ask to allow
    pyautogui.moveTo(950, 690, duration=1)
    pyautogui.click()
    pyautogui.moveTo(950, 670, duration=1)
    pyautogui.click()
    time.sleep(1.2)


if __name__ == "__main__":
    main()
