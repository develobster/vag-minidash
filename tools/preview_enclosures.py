"""Render actual STL meshes, distinct from AI industrial-design concepts."""
from pathlib import Path
import numpy as np
import trimesh
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

ROOT=Path(__file__).resolve().parents[1]
STL=ROOT/'artifacts/local/enclosures'
OUT=ROOT/'docs/review-2026-11-05'

def main():
    for device,dims in [('s3',(112,86,42)),('cyd',(102,66,32)),('lite',(108,46,24))]:
        fig=plt.figure(figsize=(15,6),facecolor='#f3f6fa')
        for i,variant in enumerate(('compact','hood','panel')):
            ax=fig.add_subplot(1,3,i+1,projection='3d',facecolor='#f3f6fa')
            for part,col in [('base','#648598'),('bezel','#b8c7ce')]:
                mesh=trimesh.load_mesh(STL/f'{device}-{variant}-{part}.stl')
                tri=mesh.triangles.copy()
                if part=='bezel':
                    tri[:,:,0]*=-1
                    tri[:,:,2]=dims[2]+2.4-tri[:,:,2]+(12 if variant=='hood' else 0)
                normal=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0])
                normal/=np.maximum(np.linalg.norm(normal,axis=1,keepdims=True),1e-9)
                light=np.array([-.3,-.5,1.]);light/=np.linalg.norm(light)
                from matplotlib.colors import to_rgb
                shade=.55+.45*np.maximum(normal@light,0)
                colors=np.asarray(to_rgb(col))[None,:]*shade[:,None]
                poly=Poly3DCollection(tri,facecolors=colors,edgecolors='none',alpha=1)
                ax.add_collection3d(poly)
            xspan=dims[0]*.63;yspan=dims[1]*.7;zspan=dims[2]+18
            ax.set_xlim(-xspan,xspan);ax.set_ylim(-yspan,yspan);ax.set_zlim(0,zspan)
            ax.set_box_aspect((xspan*2,yspan*2,zspan));ax.view_init(elev=45,azim=-65)
            ax.set_axis_off();ax.set_title(variant.upper(),fontsize=16,pad=4)
        fig.suptitle(f'MINIDASH {device.upper()} | Actual STL geometry',fontsize=23,y=.95)
        fig.text(.5,.07,f'FIT-TEST ONLY | base {dims[0]} x {dims[1]} x {dims[2]} mm | PCB dimensions not measured',ha='center',fontsize=12)
        fig.subplots_adjust(left=0,right=1,bottom=.12,top=.87,wspace=0)
        fig.savefig(OUT/f'cad-{device}.png',dpi=160);plt.close(fig)

if __name__=='__main__':main()
