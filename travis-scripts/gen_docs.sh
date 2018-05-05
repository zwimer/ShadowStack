#!/bin/sh
set -e
cd $TRAVIS_BUILD_DIR

# Do nothing if this commit should be skipped
if [ -f ./travis-scripts/is_autogenerated_and_should_skip ]; then
	exit 0
fi


# Get the current gh-pages branch
git clone -b gh-pages https://git@$GH_REPO_REF
rm -rf ./docs | true

# Generate docs
echo 'Reading version...'
PROJECT_VERSION=$(cat VERSION | awk '{$1=$1};1')
echo 'Generating Doxygen code documentation...'
( cat Doxyfile ; echo "PROJECT_NUMBER=$PROJECT_VERSION" ) | doxygen - > doxygen.log

# Move docs into the repo
echo 'Generating docs...'
cd $GH_REPO_NAME
rm -rf ./docs/* | true
mv ../doxygen.log .
mv ../docs .
touch .nojekyll

# Only upload if Doxygen successfully created the documentation.
# Check this by verifying that the html directory and the file html/index.html
# both exist. This is a good indication that Doxygen did it's work.
if [ -d "html" ] && [ -f "html/index.html" ]; then

    echo 'Uploading documentation to the gh-pages branch...'
    git add -A
    git commit -m "Deploy code docs to GitHub Pages Travis build: ${TRAVIS_BUILD_NUMBER}" -m "Commit: ${TRAVIS_COMMIT}" | true

    # The ouput is redirected to /dev/null to hide any sensitive credential data
    # that might otherwise be exposed.
    git push "https://${GH_REPO_TOKEN}@${GH_REPO_REF}" > /dev/null 2>&1
else
    echo '' >&2
    echo 'Warning: No documentation (html) files have been found!' >&2
    echo 'Warning: Not going to push the documentation to GitHub!' >&2
    exit 1
fi