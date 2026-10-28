"""Runs docs/queries.sql block by block on a temporary copy of the database and writes docs/QUERIES.md."""
import re, shutil, subprocess, sys, datetime, tempfile, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(root, 'docs', 'queries.sql')).read()
blocks = re.split(r'^-- @', src, flags=re.M)[1:]
out = ["# Demonstration queries with real output\n",
       f"Outputs were produced by running each block on a fresh database created from `schema.sql` + `sample_data.sql` on {datetime.date.today()}.",
       "Dates in the sample data are relative to the day the database was created, so your dates will differ; the numbers and structure will match.\n"]
for b in blocks:
    head, _, rest = b.partition('\n')
    title = head.strip()
    comments = [l[3:] for l in rest.splitlines() if l.startswith('-- ')]
    sql = '\n'.join(l for l in rest.splitlines() if not l.startswith('--')).strip()
    tmp = tempfile.mktemp(suffix='.db')
    shutil.copy(os.path.join(root, 'database', 'LabEquipmentDB.db'), tmp)
    r = subprocess.run(['sqlite3', '-header', '-column', tmp], input=sql, capture_output=True, text=True)
    os.remove(tmp)
    result = (r.stdout + r.stderr).rstrip()
    out.append(f"## {title.replace(' | ', ': ', 1)}\n\n" + ' '.join(comments) + f"\n\n```sql\n{sql}\n```\n\n**Output**\n\n```\n{result}\n```\n")
open(os.path.join(root, 'docs', 'QUERIES.md'), 'w').write('\n'.join(out))
print(len(blocks), 'queries written')
