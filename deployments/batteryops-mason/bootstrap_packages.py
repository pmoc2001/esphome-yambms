"""Materialize the pinned upstream packages and the qualified local overrides."""
import io
from pathlib import Path
import shutil
import subprocess
import tarfile

PIN='e183d206bc3b041fdd39dc2ee331e6ef22f4b84d'
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[1]
archive=subprocess.run(['git','-C',str(REPO),'archive',PIN,'packages'],
                        capture_output=True,check=True).stdout
with tarfile.open(fileobj=io.BytesIO(archive)) as source:
    for member in source:
        if not member.isfile():
            continue
        relative=Path(member.name).relative_to('packages')
        assert '..' not in relative.parts
        destination=ROOT/'yam-packages'/relative
        destination.parent.mkdir(parents=True,exist_ok=True)
        destination.write_bytes(source.extractfile(member).read())
for source in (ROOT/'package-overrides').rglob('*.yaml'):
    shutil.copy2(source,ROOT/'yam-packages'/source.relative_to(ROOT/'package-overrides'))
print('Pinned YamBMS packages and qualified local overrides prepared')
