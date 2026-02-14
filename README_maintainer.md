# README for project maintainers

## github / github workflows

### cpplint - Static Sourcecode Validation
On pullrequests or release builds cpplint (a static code checker for C++) is involved to check and validate the source code.
See:
* [github/workflows/cpplint.yml](./blob/workflows/.github/workflows/cpplint.yml]
* [cpplint](https://github.com/cpplint/cpplint)

To enable this feature you have to enable workflows in your repository by going to:

https://github.com/YOUR-GITHUB-USERNAME/YOUR-GITHUB-PROJECTNAME/settings/actions#:~:text=Actions%20permissions

and enable workflows.

Usually set ```Allow all actions and reusable workflows```


### Automatic builds

On pullrequests or release builds automatic builds get triggered.

See:
* [.github/workflows/build.yml](./blob/workflows/build.yml)

To enable this feature you have to enable workflows in your repository by going to:

https://github.com/YOUR-GITHUB-USERNAME/YOUR-GITHUB-PROJECTNAME/settings/actions#:~:text=Actions%20permissions

and enable workflows.

Usually set ```Allow all actions and reusable workflows```


### Release Generation
On release generation the automatic built files get attached to the release.
Also a summary of commit messages is set to the release informations which can be adapted after gerneration via github interface.

To enable this feature you have to enable workflows and also give them write-access to your repository by:
* Enable workflows
* Enable write access

Goto:
https://github.com/YOUR-GITHUB-USERNAME/YOUR-GITHUB-PROJECTNAME/settings/actions#:~:text=Actions%20permissions

and enable workflows.

Usually set ```Allow all actions and reusable workflows```

Goto: https://github.com/YOUR-GITHUB-USERNAME/YOUR-GITHUB-PROJECTNAME/settings/actions#:~:text=Workflow%20permissions

Usually set ```Read and write permissions```

jkl
