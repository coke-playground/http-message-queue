load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository')

def http_message_queue_workspace(workflow_tag=None, workflow_commit=None, workflow_path=None,
                                 coke_tag=None, coke_commit=None, coke_path=None,
                                 klog_tag=None, klog_commit=None):
    if workflow_path:
        native.local_repository(
            name = "workflow",
            path = workflow_path
        )
    elif workflow_tag or workflow_commit:
        git_repository(
            name = "workflow",
            remote = "https://github.com/sogou/workflow.git",
            tag = workflow_tag,
            commit = workflow_commit
        )

    if coke_path:
        native.local_repository(
            name = "coke",
            path = coke_path,
        )
    elif coke_tag or coke_commit:
        git_repository(
            name = "coke",
            remote = "https://github.com/kedixa/coke.git",
            tag = coke_tag,
            commit = coke_commit,
        )

    if klog_tag or klog_commit:
        git_repository(
            name = "klog",
            remote = "https://github.com/coke-playground/klog.git",
            tag = klog_tag,
            commit = klog_commit,
        )
