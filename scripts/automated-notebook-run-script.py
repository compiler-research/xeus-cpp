import argparse
from selenium import webdriver
from selenium.webdriver.chrome.options import Options as ChromeOptions
from selenium.webdriver.firefox.options import Options as FirefoxOptions
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys
import time
import subprocess

def main():
    parser = argparse.ArgumentParser(description="Run Selenium with a chosen driver")
    parser.add_argument(
        "--driver",
        type=str,
        default="chrome",
        choices=["chrome", "firefox", "safari"],
        help="Choose which WebDriver to use",
    )

    args = parser.parse_args()
    URL = "http://127.0.0.1:8000/lab/index.html?path=smallpt.ipynb"

    # This will start the right driver depending on what
    # driver option is chosen
    if args.driver == "chrome":
        options = ChromeOptions()
        options.add_argument("--headless")
        options.add_argument("--no-sandbox")
        driver = webdriver.Chrome(options=options)

    elif args.driver == "firefox":
        options = FirefoxOptions()
        options.add_argument("--headless")
        driver = webdriver.Firefox(options=options)

    elif args.driver == "safari":
        driver = webdriver.Safari()

    wait = WebDriverWait(driver, 30)
    actions = ActionChains(driver)

    # Open Jupyter Lite with the notebook requested
    driver.get(URL)

    # Waiting for Jupyter Lite URL to finish loading
    notebook_area = wait.until(
        EC.presence_of_element_located((By.CSS_SELECTOR, ".jp-Notebook"))
    )

    time.sleep(1)

    notebook_area.click()
    time.sleep(0.5)

    # This will run all the cells of the chosen notebook
    if args.driver == "chrome":
        print("Opening Run Menu")
        run_menu = wait.until(
            EC.element_to_be_clickable((By.XPATH, "//li[normalize-space()='Run']"))
        )
        actions.move_to_element(run_menu).pause(0.1).click().perform()
        time.sleep(0.5)
        print("Click on Run All Cells")
        run_all_menu = wait.until(
            EC.visibility_of_element_located(
                (By.XPATH, "//li[normalize-space()='Run All Cells']")
            )
        )
        actions.move_to_element(run_all_menu).click().perform()
        time.sleep(300)

    elif args.driver == "firefox":
        print("Opening Run Menu")
        run_menu = wait.until(
            EC.element_to_be_clickable((By.XPATH, "//li[normalize-space()='Run']"))
        )
        actions.move_to_element(run_menu).pause(0.1).click().perform()
        time.sleep(0.5)
        print("Click on Run All Cells")
        run_all_menu = wait.until(
            EC.element_to_be_clickable(
                (By.XPATH, "//li[normalize-space()='Run All Cells']")
            )
        )
        actions.move_to_element(run_all_menu).click().perform()
        time.sleep(200)

    elif args.driver == "safari":
        print("Running all cells using Shift+Enter...")
        while True:
            # Focused cell
            focused_cell = driver.find_element(
                By.CSS_SELECTOR, ".jp-Notebook-cell.jp-mod-selected"
            )

            # Get the cell content text reliably in Safari
            editor_divs = focused_cell.find_elements(
                By.CSS_SELECTOR, ".jp-InputArea-editor div"
            )
            cell_content = "".join([div.text for div in editor_divs]).strip()

            if not cell_content:
                break

            # Press Shift+Enter to run the cell
            notebook_area.send_keys(Keys.SHIFT, Keys.ENTER)
            time.sleep(0.5)

        time.sleep(600)

    if args.driver == "chrome" or args.driver == "firefox":
        print("Saving notebook")
        file_menu = wait.until(
            EC.element_to_be_clickable((By.XPATH, "//li[normalize-space()='File']"))
        )
        actions.move_to_element(file_menu).click().perform()
        save_item = wait.until(
            EC.visibility_of_element_located(
                (By.XPATH, "//li[contains(normalize-space(), 'Save')]")
            )
        )

        actions.move_to_element(save_item).click().perform()
        time.sleep(0.5)

    elif args.driver == "safari":
        print("Saving notebook using command + s + enter.")
        notebook_area.send_keys(Keys.COMMAND, "s")
        time.sleep(0.5)

        notebook_area.send_keys(Keys.ENTER)
        time.sleep(0.5)

    # This downloads the notebook, so it can be compared
    # to a reference notebook
    print("Downloading notebook by clicking download button")
    search_script = """
    function deepQuerySelector(root, selector) {
    const walker = document.createTreeWalker(
    root,
    NodeFilter.SHOW_ELEMENT,
    {
    acceptNode: node => NodeFilter.FILTER_ACCEPT
    },
    false
    );
    
    while (walker.nextNode()) {
    let node = walker.currentNode;
    
    // Check if this node matches
    if (node.matches && node.matches(selector)) {
    return node;
    }
    
    // If this element has a shadow root, search inside it
    if (node.shadowRoot) {
    const found = deepQuerySelector(node.shadowRoot, selector);
    if (found) return found;
    }
    }
    return null;
    }
    
    return deepQuerySelector(document, "jp-button[data-command='docmanager:download']");
    """

    download_button = WebDriverWait(driver, 20).until(
    lambda d: d.execute_script(search_script)
    )

    print("Found element:", download_button)

    time.sleep(20)
    driver.execute_script("""
const el = arguments[0];

function fire(type) {
    el.dispatchEvent(new MouseEvent(type, {
        bubbles: true,
        cancelable: true,
        composed: true,
        view: window
    }));
}

el.scrollIntoView({ block: 'center', inline: 'center' });
el.focus({ preventScroll: true });

fire('pointerdown');
fire('mousedown');
fire('mouseup');
fire('click');
""", download_button)

    time.sleep(20)

    # Close browser
    driver.quit()


if __name__ == "__main__":
    main()
