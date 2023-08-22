# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'Xeus-Cpp'
copyright = '2023,Compiler Research'
author = 'Xeus-Cpp Contributors'
release = 'Dev'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = []

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']



# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

# html_theme = 'alabaster'

html_theme_options = {
    "github_user": "Xeus-Cpp Contributors",
    "github_repo": "Xeus-Cpp",
    "github_banner": True,
    "fixed_sidebar": True,
}

todo_include_todos = True

mathjax_path = "https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js"

# Add latex physics package
mathjax3_config = {
    "loader": {"load": ["[tex]/physics"]},
    "tex": {"packages": {"[+]": ["physics"]}},
}

import os
import sphinx_rtd_theme
import subprocess

XEUS_CPP_ROOT = os.path.abspath('..')
html_extra_path = [XEUS_CPP_ROOT + '/build/docs/']

command = 'mkdir {0}/build; cd {0}/build; cmake ../ -DXEUS_CPP_INCLUDE_DOCS=ON'.format(XEUS_CPP_ROOT)
subprocess.call(command, shell=True)
subprocess.call('doxygen {0}/build/docs/doxygen.cfg'.format(XEUS_CPP_ROOT), shell=True)

on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

# if on_rtd:
#     subprocess.call('cd ..; doxygen', shell=True)

html_theme = "sphinx_rtd_theme"

html_theme_path = [sphinx_rtd_theme.get_html_theme_path()]

extensions = ['breathe']
breathe_projects = { 'xeus-cpp': '../xml' }
templates_path = ['_templates']
html_static_path = ['_static']
source_suffix = '.rst'
master_doc = 'index'
project = 'xeus-cpp'
copyright = '2023, xeus-cpp Contributors'
author = 'Xeus-cpp Contributors'

exclude_patterns = []
highlight_language = 'C++'
pygments_style = 'sphinx'
todo_include_todos = False
htmlhelp_basename = ''
