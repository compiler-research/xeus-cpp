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
import platform
import sys


def cell_is_waiting_for_input(driver):
    dialog_selectors = [".jp-Stdin-input"]

    for selector in dialog_selectors:
        try:
            elems = driver.find_elements(By.CSS_SELECTOR, selector)
            if any(elem.is_displayed() for elem in elems):
                return True
        except Exception:
            pass

    return False


def main():
    parser = argparse.ArgumentParser(description="Run Selenium with a chosen driver")
    parser.add_argument(
        "--driver",
        type=str,
        default="chrome",
        choices=["chrome", "firefox", "safari"],
        help="Choose which WebDriver to use",
    )
    parser.add_argument(
        "--notebook",
        type=str,
        required=True,
        help="Notebook to execute",
    )
    parser.add_argument(
        "--kernel",
        type=str,
        required=True,
        help="Kernel to run notebook in",
    )
    parser.add_argument(
        "--stdin",
        type=str,
        help="Text to pass to standard input",
    )

    args = parser.parse_args()
    URL = f"http://127.0.0.1:8000/lab/index.html?path={args.notebook}"

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
    actions.context_click(notebook_area).pause(0.01).send_keys(Keys.DOWN * 9).pause(0.01).send_keys(Keys.ENTER).pause(0.01).perform()

    # Select Kernel based on input
    kernel_button = driver.find_element(
        By.CSS_SELECTOR, "jp-button.jp-Toolbar-kernelName.jp-ToolbarButtonComponent"
    )
    driver.execute_script("arguments[0].click();", kernel_button)
    driver.switch_to.active_element.send_keys(Keys.TAB)
    time.sleep(0.01)
    actions.send_keys(f"{args.kernel}").perform()
    time.sleep(0.01)
    actions.send_keys(Keys.TAB).perform()
    time.sleep(0.01)
    actions.send_keys(Keys.ENTER).perform()
    time.sleep(0.01)

    # This will run all the cells of the chosen notebook
    print("Running Cells")
    while True:
        focused_cell = driver.find_element(
            By.CSS_SELECTOR, ".jp-Notebook-cell.jp-mod-selected"
        )
        editor_divs = focused_cell.find_elements(
            By.CSS_SELECTOR, ".jp-InputArea-editor div"
        )

        cell_content = "".join(
            div.get_attribute("textContent") for div in editor_divs
        ).strip()

        if not cell_content:
            print("Empty cell reached")
            break

        if cell_is_waiting_for_input(driver):
            print("Cell requesting input")
            input_box = WebDriverWait(driver, 5).until(
                EC.visibility_of_element_located(
                    (By.CSS_SELECTOR, ".jp-Stdin-input")
                )
            )
            input_box.click()
            input_box.send_keys(f"{args.stdin}")
            time.sleep(0.1)
            input_box.send_keys(Keys.CONTROL, Keys.ENTER)
            next_cell = focused_cell.find_element(
                By.XPATH,
                "following-sibling::div[contains(@class,'jp-Notebook-cell')][1]",
            )
            driver.execute_script(
                "arguments[0].scrollIntoView({block:'center'});", next_cell
            )
            next_cell.click()
            if args.driver == "safari":
                driver.execute_script(
                    """
                    const evt = new KeyboardEvent('keydown', {
                    key: 'Enter',
                    code: 'Enter',
                    keyCode: 13,
                    which: 13,
                    shiftKey: true,
                    bubbles: true
                    });
                    document.activeElement.dispatchEvent(evt);
                    """
                )
            while True:
                spans = driver.find_elements(By.CSS_SELECTOR, "span.jp-StatusBar-TextItem")
                status_span = spans[2]
                text = status_span.text
                
                if "Idle" in text:
                    break
                time.sleep(0.01)
            print(focused_cell.text)
            focused_cell=next_cell

        notebook_area.send_keys(Keys.SHIFT, Keys.ENTER)
        time.sleep(0.1)
        if not cell_is_waiting_for_input(driver):
            while True:
                spans = driver.find_elements(By.CSS_SELECTOR, "span.jp-StatusBar-TextItem")
                status_span = spans[2]
                text = status_span.text

                if "Idle" in text:
                    print(focused_cell.text)
                    break
                time.sleep(0.01)

    # In case the notebook stalls during execution
    # this makes it so the notebook moves onto the
    # save stage after 600 seconds even if the kernel
    # is still busy
    timeout = 360
    start_time = time.time()
    while True:
        elapsed = time.time() - start_time
        if elapsed > timeout:
            print(f"Timeout reached ({elapsed:.1f} seconds). Stopping loop.")
            sys.exit(1)

        spans = driver.find_elements(By.CSS_SELECTOR, "span.jp-StatusBar-TextItem")
        status_span = spans[2]
        text = status_span.text

        print(f"[{elapsed:.1f}s] Kernel status: {text}")

        if "Idle" in text:
            print("Kernel is Idle. Stopping loop.")
            break

        time.sleep(2)

    if args.driver == "chrome":
        print("Saving notebook using command + s + enter.")
        actions.send_keys(Keys.COMMAND, "s")
        time.sleep(0.5)

        actions.send_keys(Keys.ENTER)
        time.sleep(0.5)

    elif args.driver == "safari" or args.driver == "firefox":
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

    download_button = driver.execute_script(search_script)

    time.sleep(1)
    driver.execute_script(
        """
        const el = arguments[0];
        
        // Force element to be visible and focused
        el.scrollIntoView({block: 'center', inline: 'center'});
        
        // Dispatch real mouse events since Safari WebDriver ignores .click() on Web Components
        ['pointerdown', 'mousedown', 'mouseup', 'click'].forEach(type => {
        el.dispatchEvent(new MouseEvent(type, {
        bubbles: true,
        cancelable: true,
        composed: true,   // IMPORTANT for shadow DOM
        view: window
        }));
        });
        """,
        download_button,
    )

    time.sleep(1)

    # Close browser
    driver.quit()


if __name__ == "__main__":
    main()
