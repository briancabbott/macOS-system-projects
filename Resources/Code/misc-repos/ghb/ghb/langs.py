#
# Get the language breakdown for a repo
# Usage: ghb langs USER/REPO
#
import operator
import sys

import requests

from .helpers import credentials

URL = "https://api.github.com/repos/%s/languages"


def average(total, number):
    return round((number / float(total)) * 100, 2)


def main(args):
    username, password = credentials.credentials()
    headers = {"Accept": "application/vnd.github.v3+json"}
    r = requests.get(
        URL % args.repo, auth=(username, password), headers=headers
    )
    response_json = r.json()
    if r.status_code != 200:
        sys.exit("Failed with error: %s" % (response_json["message"]))
    total = sum(response_json.values())
    averages = {k: average(total, v) for k, v in response_json.items()}
    averages = sorted(
        averages.items(), key=operator.itemgetter(1), reverse=True
    )
    for t in averages:
        print(f"{t[0]:>15}: {t[1]:8.2f}%")
