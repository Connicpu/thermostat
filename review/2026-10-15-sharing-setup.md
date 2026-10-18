Record of the repo's public-sharing setup, applied on 2026-10-15.

# Making the repo public (checklist)

This file is a runbook. It was applied on 2026-10-15: `main` at `85b30cf`, Issues/Discussions/
Wiki/Projects/PRs off, ruleset `main-admin-only` active, repo public.

## 1. Push this branch and merge to main when ready

Local prep is on branch `prep/public`. Review, then fast-forward `main`. Do **not** force-push.

## 2. Lock down contribution surfaces (before or right after visibility flip)

Connie wants sharing only: no Issues, no Discussions, no PRs.

### UI

1. Repo **Settings → General → Features**
2. Uncheck **Issues**
3. Uncheck **Discussions** (if checked)
4. Uncheck **Pull requests** (GitHub added full disable in Feb 2026 — see
   https://github.blog/changelog/2026-02-13-new-repository-settings-for-configuring-pull-request-access/
   and https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/disabling-pull-requests)
5. Optionally uncheck Wiki / Projects

### CLI / API (verified against this repo’s GraphQL schema)

```bash
# Issues + Discussions + Wiki + Projects (REST / gh)
gh repo edit Connicpu/thermostat \
  --enable-issues=false \
  --enable-discussions=false \
  --enable-wiki=false \
  --enable-projects=false

# Pull requests: GraphQL UpdateRepository (hasPullRequestsEnabled).
# As of the gh version on this box, `gh repo edit` has no --enable-pull-requests flag yet;
# use GraphQL:
REPO_ID=$(gh api graphql -f query='query($o:String!,$n:String!){
  repository(owner:$o,name:$n){id}}' -F o=Connicpu -F n=thermostat --jq .data.repository.id)

gh api graphql -f query='mutation($id:ID!){
  updateRepository(input:{repositoryId:$id, hasPullRequestsEnabled:false}) {
    repository { hasPullRequestsEnabled hasIssuesEnabled hasDiscussionsEnabled }
  }
}' -F id="$REPO_ID"
```

REST `PATCH /repos/{owner}/{repo}` also exposes `has_issues`; the repo JSON includes
`has_pull_requests` (readable). Prefer GraphQL `hasPullRequestsEnabled` for the PR kill switch.

Optional: `pullRequestCreationPolicy` can restrict PR creation to collaborators only if she
ever wants PRs from herself but not from the public.

### Branch ruleset (optional belt-and-suspenders)

If PRs cannot be fully disabled on her plan/UI, a ruleset on `main` that **blocks all
non-fast-forward updates** except admins still allows forks to *open* PRs that simply never
merge. Prefer the Features → Pull requests off switch above.

Example ruleset via UI: **Settings → Rules → Rulesets → New** → target `main` →
block deletions, require linear history, restrict updates to admins. Or:

```bash
# Illustrative only — adjust after reading current Rules API docs
gh api repos/Connicpu/thermostat/rulesets --method POST --input - <<'JSON'
{
  "name": "main-admin-only",
  "target": "branch",
  "enforcement": "active",
  "conditions": { "ref_name": { "include": ["refs/heads/main"], "exclude": [] } },
  "rules": [
    { "type": "deletion" },
    { "type": "update" },
    { "type": "non_fast_forward" }
  ],
  "bypass_actors": [
    { "actor_id": 5, "actor_type": "RepositoryRole", "bypass_mode": "always" }
  ]
}
JSON
```

The bypass actor is required: without it the `update` rule blocks Connie's own pushes to
`main`. `actor_id: 5` is the built-in **Admin** repository role (Connie, as owner, holds it),
so she can push while nobody else can. Check afterwards with
`gh api repos/Connicpu/thermostat/rulesets` that `bypass_actors` lists the Admin role.

## 3. Flip visibility

```bash
# Only when she explicitly asks — destructive consequences for stars/forks later.
# The gh on the prep box has no --accept-visibility-change-consequences flag, so use REST:
gh api -X PATCH repos/Connicpu/thermostat -f visibility=public --jq '{visibility,private}'
# Check as a logged-out visitor (expect 200):
curl -s -o /dev/null -w '%{http_code}\n' https://github.com/Connicpu/thermostat
```

## 4. After public

- Confirm Actions still green on a no-op push or workflow_dispatch.
- Confirm HACS / release assets on `card-v*` tags still attach.
- Do **not** `git push --all` (local-only stale branches exist on the prep machine).
