import argparse
import signal
import sys

from . import __version__
from . import approve
from . import assignme
from . import block
from . import clear_comments
from . import close_prs
from . import comment
from . import contributions
from . import create
from . import delete_branches
from . import download_release
from . import get_blocks
from . import greenify
from . import langs
from . import ls_notifications
from . import me
from . import notifications
from . import pr
from . import protect
from . import unblock
from . import unwatch
from . import watch


def _signal_handle(sig, frame):
    sys.exit(0)


def _build_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-v", "--version", action="version", version="%(prog)s " + __version__
    )

    subparsers = parser.add_subparsers(dest="subcommand")
    subparsers.required = True

    approve_parser = subparsers.add_parser("approve", help="Approve a PR")
    approve_parser.add_argument("pr", help="the PR to approve")

    assignme_parser = subparsers.add_parser(
        "assignme", help="Assign yourself to a PR"
    )
    assignme_parser.add_argument("pr", help="The PR to assign yourself to")

    block_parser = subparsers.add_parser("block", help="block a given user")
    block_parser.add_argument("user", help="the user to block")

    unblock_parser = subparsers.add_parser(
        "unblock", help="unblock a given user"
    )
    unblock_parser.add_argument("user", help="the user to unblock")

    comment_parser = subparsers.add_parser(
        "comment", help="Comment on PRs / issues"
    )
    comment_parser.add_argument("body", help="the comment body")
    comment_parser.add_argument(
        "issues",
        nargs="+",
        help="the list of issues / PRs to comment on, start with @ to use a file list",
    )

    clear_comments_parser = subparsers.add_parser(
        "clear-comments", help="Delete all comments on a PR"
    )
    clear_comments_parser.add_argument("repo", help="the user/repo to edit")
    clear_comments_parser.add_argument("pr", help="the PR number to clear")

    create_parser = subparsers.add_parser(
        "create", help="Create a new GitHub repo"
    )
    create_parser.add_argument("name", help="the name of the repo")
    create_parser.add_argument(
        "-d", "--description", help="the description of the repo"
    )
    create_parser.add_argument(
        "-u", "--url", help="the homepage for the repo", dest="homepage"
    )
    create_parser.add_argument(
        "-p",
        "--private",
        action="store_true",
        help="make the repo private",
        default=False,
    )
    create_parser.add_argument(
        "-w",
        "--wiki",
        action="store_true",
        help="enable wikis",
        default=False,
        dest="has_wiki",
    )
    create_parser.add_argument(
        "--no-issues",
        action="store_false",
        help="disable issues",
        default=True,
        dest="has_issues",
    )
    create_parser.add_argument(
        "--no-downloads",
        action="store_false",
        help="disable downloads",
        default=True,
        dest="has_downloads",
    )

    download_release_parser = subparsers.add_parser(
        "download-release", help="Download the most recent release of a repo"
    )
    download_release_parser.add_argument("repo", help="the user/repo")
    download_release_parser.add_argument(
        "-f",
        "--filename",
        action="store_true",
        help="only print the filename",
        default=False,
    )

    langs_parser = subparsers.add_parser(
        "langs", help="Get the language breakdown for a repo"
    )
    langs_parser.add_argument("repo", help="The user/repo")

    protect_parser = subparsers.add_parser(
        "protect", help="Protect/Unprotect a branch"
    )
    protect_parser.add_argument(
        "--disable",
        help="disable protection",
        action="store_true",
        default=False,
    )
    protect_parser.add_argument("repo", help="the user/repo to edit")
    protect_parser.add_argument("branch", help="The name of the branch")
    protect_parser.add_argument(
        "statuses", nargs="*", help="The required status checks"
    )

    pr_parser = subparsers.add_parser("pr", help="Create a PR")
    pr_parser.add_argument(
        "-d",
        "--draft",
        action="store_true",
        help="Create the PR as a draft",
        default=False,
    )
    pr_parser.add_argument(
        "--no-edit",
        action="store_true",
        help="Don't prompt for editing the PR message, automatically submit",
        default=False,
    )
    pr_parser.add_argument(
        "--no-open",
        action="store_true",
        help="Don't open the PR in the browser",
        default=False,
    )
    pr_parser.add_argument(
        "--merge-rebase",
        action="store_true",
        help="Enable auto-merge on the PR (rebase mode)",
        default=False,
    )
    pr_parser.add_argument(
        "--merge-squash",
        action="store_true",
        help="Enable auto-merge on the PR (squash mode using initial commit message)",
        default=False,
    )
    pr_parser.add_argument(
        "branch",
        help="The branch to base the PR on",
        nargs="?",
    )

    unwatch_parser = subparsers.add_parser(
        "unwatch", help="Unwatch GitHub repos"
    )
    unwatch_parser.add_argument(
        "-u",
        "--users",
        help="comma separated valid users. Repos from these users are never unwatched",
        default="",
    )
    unwatch_parser.add_argument(
        "-i",
        "--ignored",
        help="command separated ignored repo names. "
        "Repos from this list are automatically unwatched",
        default="",
    )

    watch_parser = subparsers.add_parser("watch", help="Watch GitHub repos")
    watch_parser.add_argument("repo", help="the user/repo to watch")

    delete_branches_parser = subparsers.add_parser(
        "delete-branches", help="Delete branches without open PRs"
    )
    delete_branches_parser.add_argument(
        "repo",
        help="The repo to get open PRs from, ex: keith/ghb",
    )
    delete_branches_parser.add_argument(
        "branch_prefixes",
        nargs="+",
        help="The prefixes of the branches to delete, ex: ci-pr-",
    )

    subparsers.add_parser(
        "contributions", help="Get your contribution count for today"
    )
    subparsers.add_parser(
        "get-blocks", help="Get list of users you've blocked"
    )
    subparsers.add_parser(
        "ls-notifications", help="Show your unread notifications"
    )
    subparsers.add_parser("me", help="Open your profile")
    subparsers.add_parser("notifications", help="Open unread notifications")

    greenify_parser = subparsers.add_parser(
        "greenify", help="Make a commit green by editing the statuses"
    )
    greenify_parser.add_argument("repo", help="the user/repo to edit")
    greenify_parser.add_argument("sha", help="the sha with the statuses")

    close_prs_parser = subparsers.add_parser(
        "close-prs", help="Bulk close pull requests matching criteria"
    )
    close_prs_parser.add_argument(
        "repo", help="The GitHub repo, ex: owner/repo"
    )
    close_prs_parser.add_argument(
        "author", help="The GitHub username of the PR author, ex: keith"
    )
    close_prs_parser.add_argument(
        "--base", help="The base branch the PRs are targeting, ex: main"
    )
    close_prs_parser.add_argument(
        "--older-than-weeks",
        help="Close PRs older than the given date in weeks",
        type=int,
    )
    return parser


def main():
    args = _build_parser().parse_args()
    commands = {
        "approve": approve.main,
        "assignme": assignme.main,
        "block": block.main,
        "clear-comments": clear_comments.main,
        "close-prs": close_prs.main,
        "comment": comment.main,
        "contributions": contributions.main,
        "create": create.main,
        "delete-branches": delete_branches.main,
        "download-release": download_release.main,
        "get-blocks": get_blocks.main,
        "greenify": greenify.main,
        "langs": langs.main,
        "ls-notifications": ls_notifications.main,
        "me": me.main,
        "notifications": notifications.main,
        "pr": pr.main,
        "protect": protect.main,
        "unblock": unblock.main,
        "unwatch": unwatch.main,
        "watch": watch.main,
    }

    signal.signal(signal.SIGINT, _signal_handle)
    commands[args.subcommand](args)


if __name__ == "__main__":
    main()
