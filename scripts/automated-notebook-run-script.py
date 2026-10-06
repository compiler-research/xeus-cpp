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
import sys


def cell_is_waiting_for_input(driver):
    """
    This function returns true if Jupyter is currently waiting the user to enter
    text in a box.
    """
    try:
        return driver.find_element(By.CSS_SELECTOR, ".jp-Stdin-input").is_displayed()
    except Exception:
        pass

    return False


def wait_for_idle_status(driver, current_cell, start_time, timeout):
    """
    This function checks whether the kernel is Idle. Used to decide when to move
    onto executing the next cell. The 0.01 seconds sleep between checks, is to limit
    the number of checks per second. After the kernel has gone Idle, we print the contents
    of the cell (source and output) to the terminal.
    """
    while (
        "Idle"
        not in driver.find_elements(By.CSS_SELECTOR, "span.jp-StatusBar-TextItem")[
            2
        ].text
    ):
        elapsed = time.time() - start_time
        # This timeout is provided in case the notebppks stalls during
        # its execution.
        if elapsed > timeout:
            print(
                f"Timeout reached ({elapsed:.1f} seconds). Stopping Notebook execution."
            )
            sys.exit(1)
        time.sleep(0.01)

    print(current_cell.text)

def rewind_to_first_cell(cell):
    while True:
        prev = cell.find_elements(
            By.XPATH,
            "preceding-sibling::div[contains(@class,'jp-Notebook-cell')][1]"
        )
        if not prev:
            return cell
        cell = prev[0]

    

def run_notebook(driver, notebook_area, args):
    """This functions runs all the cells of the notebook"""
    print("Running Cells")
    start_time = time.time()
    current_cell = driver.find_element(
        By.CSS_SELECTOR, ".jp-Notebook-cell.jp-mod-selected"
    )

    current_cell = rewind_to_first_cell(current_cell)
    
    while True:
        editor_divs = current_cell.find_elements(
            By.CSS_SELECTOR, ".jp-InputArea-editor div"
        )

        cell_content = "".join(
            div.get_attribute("textContent") for div in editor_divs
        ).strip()

        if not cell_content:
            # An empty cell is used to determine the end of the notebook
            # that is being executed.
            print("Empty cell reached")
            break

        if cell_is_waiting_for_input(driver):
            print("Cell requesting input")
            current_cell = driver.find_element(
                By.CSS_SELECTOR, ".jp-Notebook-cell.jp-mod-selected"
            )
            input_box = WebDriverWait(driver, 5).until(
                EC.visibility_of_element_located((By.CSS_SELECTOR, ".jp-Stdin-input"))
            )
            input_box.click()
            input_box.send_keys(f"{args.stdin}")
            time.sleep(0.25)
            input_box.send_keys(Keys.CONTROL, Keys.ENTER)
            next_cell = current_cell.find_element(
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
            wait_for_idle_status(driver, current_cell, start_time, args.timeout)
            current_cell = next_cell
            time.sleep(0.25)

        notebook_area.send_keys(Keys.SHIFT, Keys.ENTER)
        # This sleep is there is allow time for the box for standard input
        # to appear, if it needs to after executing the cell
        time.sleep(0.25)
        if not cell_is_waiting_for_input(driver):
            wait_for_idle_status(driver, current_cell, start_time, args.timeout)
            next_cell = current_cell.find_element(
                By.XPATH,
                "following-sibling::div[contains(@class,'jp-Notebook-cell')][1]",
            )
            current_cell = next_cell

def download_notebook(driver):
    """This function is used to download the notebook currently open."""
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


def choose_kernel(driver, args):
    """This function sets the kernel based on the user input."""
    kernel_button = driver.find_element(
        By.CSS_SELECTOR, "jp-button.jp-Toolbar-kernelName.jp-ToolbarButtonComponent"
    )
    driver.execute_script("arguments[0].click();", kernel_button)
    driver.switch_to.active_element.send_keys(Keys.TAB)
    time.sleep(0.01)
    ActionChains(driver).send_keys(f"{args.kernel}").perform()
    time.sleep(0.01)
    ActionChains(driver).send_keys(Keys.TAB).perform()
    time.sleep(0.01)
    ActionChains(driver).send_keys(Keys.ENTER).perform()
    time.sleep(0.01)


def save_notebook(notebook_area):
    """This function saves the notebook."""
    print("Saving the notebook")
    notebook_area.send_keys(Keys.COMMAND, "s")
    time.sleep(0.5)


def clear_notebook_output(driver, notebook_area):
    """
    This function clears the output of all cells in the notebook, before it is
    executed by run_notebook.
    """
    ActionChains(driver).context_click(notebook_area).pause(0.01).send_keys(
        Keys.DOWN * 9
    ).pause(0.01).send_keys(Keys.ENTER).pause(0.01).perform()


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
    parser.add_argument(
        "--timeout",
        type=int,
        default=120,
        help="Maximum time (in seconds) allowed for notebook execution before timeout.",
    )
    parser.add_argument(
        "--run-browser-gui",
        action="store_true",
        help="Run browser with a visible GUI (disables headless mode).",
    )
    
    args = parser.parse_args()
    URL = f"http://127.0.0.1:8000/lab/index.html?path={args.notebook}"

    # This will start the right driver depending on what
    # driver option is chosen
    if args.driver == "chrome":
        options = ChromeOptions()
        if not args.run_browser_gui:
            options.add_argument("--headless")
            options.add_argument("--no-sandbox")
        driver = webdriver.Chrome(options=options)

    elif args.driver == "firefox":
        options = FirefoxOptions()
        if not args.run_browser_gui:
            options.add_argument("--headless")
        driver = webdriver.Firefox(options=options)

    elif args.driver == "safari":
        driver = webdriver.Safari()

    wait = WebDriverWait(driver, 30)

    # Open Jupyter Lite with the notebook requested
    driver.get(URL)

    # Waiting for Jupyter Lite URL to finish loading
    notebook_area = wait.until(
        EC.presence_of_element_located((By.CSS_SELECTOR, ".jp-Notebook"))
    )

    # Without this sleep, the ci will fail for Safari will fail
    # Unable to currently determine root cause. This is not needed
    # locally.
    time.sleep(1)

    # This clears the output of the reference notebook
    # before executing it, so that when we download it,
    # we know the output is purely from the ci running
    # the notebook.
    clear_notebook_output(driver, notebook_area)

    # Select Kernel based on input
    choose_kernel(driver, args)
        
    # This will run all the cells of the chosen notebook
    run_notebook(driver, notebook_area, args)

    # This section saves the notebook,
    save_notebook(notebook_area)

    # This section downloads the notebook, so it can be compared
    # to a reference notebook
    download_notebook(driver)

    # Close browser
    driver.quit()


if __name__ == "__main__":
    main()
