#
# Bulk close pull requests matching some criteria. This is especially useful
# for cleaning up old automated pull requests and can also be used with ghb
# delete-branches
#
# Usage: ghb close-prs author base
#
import argparse
import datetime
import itertools
import json
import sys
from concurrent import futures
from typing import Optional
from typing import Set

import requests

from .helpers import credentials


def _get_open_prs(
    user: str,
    password: str,
    repo: str,
    author: str,
    base: Optional[str],
    older_than_weeks: Optional[int],
) -> Set[str]:
    if not base and not older_than_weeks:
        raise SystemExit(
            "At least one of --base or --older-than must be specified"
        )

    params = {"state": "open", "per_page": "100"}
    if base:
        params["base"] = base
    url = f"https://api.github.com/repos/{repo}/pulls"
    urls = set()
    while url:
        response = requests.get(url, auth=(user, password), params=params)
        if response.status_code != 200:
            raise SystemExit(f"error: failed to fetch PRs: {response.text}")

        for pr in response.json():
            if base:
                assert pr["base"]["ref"] == base
            assert pr["state"] == "open"

            if older_than_weeks:
                reference_date = datetime.datetime.now() - datetime.timedelta(
                    weeks=older_than_weeks
                )
                updated_at = datetime.datetime.strptime(
                    pr["created_at"], "%Y-%m-%dT%H:%M:%SZ"
                )
                if updated_at > reference_date:
                    continue

            if pr["user"]["login"].lower() == author.lower():
                urls.add(pr["url"])

        url = response.links.get("next", {}).get("url", None)

    return urls


def _close_pr(arg):
    pr_url, auth = arg
    data = json.dumps({"state": "closed"})
    response = requests.patch(pr_url, auth=auth, data=data)
    return response.status_code, pr_url


def main(args: argparse.Namespace) -> None:
    user, password = credentials.credentials()
    open_prs = _get_open_prs(
        user,
        password,
        args.repo,
        args.author,
        args.base,
        args.older_than_weeks,
    )
    failed = False
    with futures.ProcessPoolExecutor() as pool:
        for status_code, pr_url in pool.map(
            _close_pr, zip(open_prs, itertools.repeat((user, password)))
        ):
            if status_code != 200:
                print(f"warning: failed to close {pr_url}: {status_code}")
                failed = True

    if failed:
        sys.exit(1)
