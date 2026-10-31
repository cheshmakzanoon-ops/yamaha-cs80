from pathlib import Path
import csv,numpy as np,matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path('/workspace/scratch/2f805bffd21a/cs80-ui-publication/docs/evidence/continuation-2026-10-31/bass-continuity-3226011')
raw=Path('/tmp/cs80-bass-continuity-3226011/traced')
metrics=list(csv.DictReader((root/'metrics.csv').open()))
fig,axs=plt.subplots(1,2,figsize=(11,4.3),sharey=True)
for ax,m in zip(axs,metrics):
 rate=int(m['rate']);data=np.fromfile(raw/f'audio-{rate}.bin',dtype=np.float32).reshape(2,-1);ref=float(m['chord_reference']);win=round(.008*rate)
 for name,key,color in [('Retrigger','event_frame','#155a9a'),('First strike','first_strike_event','#b95e08')]:
  at=int(m[key]);v=np.max(np.abs(data[:,at:at+win]-data[:,at-1:at+win-1]),axis=0).astype(np.float64)/ref
  x=np.arange(win)*1000/rate;ax.plot(x,v,label=name,color=color,linewidth=1.3)
  i=int(np.argmax(v));ax.plot(x[i],v[i],'o',color=color,markersize=4)
  ax.annotate(f'{v[i]:.6f}',(x[i],v[i]),textcoords='offset points',xytext=(-10,9),ha='right',color=color,fontsize=10)
 ax.axhline(1,color='#727780',linestyle='--',linewidth=1,label='Existing limit / chord reference')
 ax.set_title(f'{rate/1000:g} kHz',fontsize=12,fontweight='bold')
 ax.set_xlabel('Time since note-on (ms)');ax.set_xlim(0,8);ax.set_ylim(0,1.65);ax.grid(alpha=.16)
 ax.spines[['top','right']].set_visible(False)
axs[0].set_ylabel('Largest channel step / chord reference')
handles,labels=axs[0].get_legend_handles_labels();fig.legend(handles,labels,loc='lower center',ncol=3,frameon=False,bbox_to_anchor=(.5,.025))
fig.suptitle('Bass Stack: original physical-PANEL phrase scores',fontsize=15,fontweight='bold',y=.99)
fig.text(.5,.91,'Software trace • source performance policy • unchanged 8 ms score window',ha='center',fontsize=10,color='#555b65')
fig.subplots_adjust(left=.085,right=.98,top=.82,bottom=.24,wspace=.15)
fig.savefig(root/'phrase-step-comparison.svg',metadata={'Date':None})
fig.savefig('/tmp/cs80-bass-continuity-3226011/phrase-step-comparison.png',dpi=150)
(root/'plot.py').write_text(Path(__file__).read_text())
